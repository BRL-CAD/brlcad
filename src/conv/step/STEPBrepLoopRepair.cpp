/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepLoopRepair.cpp
 *
 * Periodic-loop branch, endpoint, orientation, and face-bound repair.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

size_t
repair_exact_periodic_loop_branches(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    struct BranchState {
	int shift[2];
	ON_3dPoint start;
	ON_3dPoint end;
    };
    struct ShiftedCurve {
	ON_BrepTrim *trim;
	ON_Curve *curve;
    };

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	if (face_source_tags && (!face || face_source_tags->find(
		face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 2 ||
		(!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;

	const double period[2] = {
	    surface->IsClosed(0) ? surface->Domain(0).Length() : 0.0,
	    surface->IsClosed(1) ? surface->Domain(1).Length() : 0.0
	};
	double parameter_tolerance = ON_ZERO_TOLERANCE;
	for (int axis = 0; axis < 2; ++axis) {
	    if (period[axis] > ON_ZERO_TOLERANCE)
		parameter_tolerance = std::max(parameter_tolerance,
		    period[axis] * 1.0e-10);
	}

	/* Only solve a parameter-branch discontinuity whose source topology and
	 * 3-D surface lifts already prove that every adjacent pair shares the
	 * asserted STEP vertex.  This prevents periodicity from hiding a real gap. */
	bool exact_topology = true;
	double original_cost = 0.0;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count()) {
		exact_topology = false;
		break;
	    }
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge())
		tolerance = std::max(tolerance, previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > tolerance ||
		    next_lift.DistanceTo(vertex) > tolerance) {
		exact_topology = false;
		break;
	    }
	    original_cost += previous_uv.DistanceTo(next_uv);
	}
	if (!exact_topology || original_cost <= parameter_tolerance)
	    continue;

	std::vector<ON_BrepTrim *> trims;
	std::vector<std::vector<BranchState> > states;
	trims.reserve(loop.TrimCount());
	states.resize(loop.TrimCount());
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim) {
		exact_topology = false;
		break;
	    }
	    trims.push_back(trim);
	    int fixed_direction = -1;
	    if (trim->m_type == ON_BrepTrim::seam ||
		    trim->m_type == ON_BrepTrim::singular) {
		if (trim->m_iso == ON_Surface::W_iso ||
			trim->m_iso == ON_Surface::E_iso)
		    fixed_direction = 0;
		else if (trim->m_iso == ON_Surface::S_iso ||
			trim->m_iso == ON_Surface::N_iso)
		    fixed_direction = 1;
	    }
	    const bool special = trim->m_type == ON_BrepTrim::seam ||
		trim->m_type == ON_BrepTrim::singular;
	    const bool fixed0 = lti == 0 || !(period[0] > ON_ZERO_TOLERANCE) ||
		(special && fixed_direction != 1);
	    const bool fixed1 = lti == 0 || !(period[1] > ON_ZERO_TOLERANCE) ||
		(special && fixed_direction != 0);
	    const int minimum0 = fixed0 ? 0 : -kMaximumPeriodicBranchShift;
	    const int maximum0 = fixed0 ? 0 : kMaximumPeriodicBranchShift;
	    const int minimum1 = fixed1 ? 0 : -kMaximumPeriodicBranchShift;
	    const int maximum1 = fixed1 ? 0 : kMaximumPeriodicBranchShift;
	    for (int shift0 = minimum0; shift0 <= maximum0; ++shift0) {
		for (int shift1 = minimum1; shift1 <= maximum1; ++shift1) {
		    BranchState state;
		    state.shift[0] = shift0;
		    state.shift[1] = shift1;
		    state.start = trim->PointAtStart();
		    state.end = trim->PointAtEnd();
		    state.start.x += shift0 * period[0];
		    state.end.x += shift0 * period[0];
		    state.start.y += shift1 * period[1];
		    state.end.y += shift1 * period[1];
		    states[lti].push_back(state);
		}
	    }
	}
	if (!exact_topology)
	    continue;

	const size_t count = trims.size();
	std::vector<std::vector<double> > costs(count);
	std::vector<std::vector<int> > predecessors(count);
	for (size_t position = 0; position < count; ++position) {
	    costs[position].assign(states[position].size(), DBL_MAX);
	    predecessors[position].assign(states[position].size(), -1);
	}
	costs[0][0] = 0.0;
	for (size_t position = 1; position < count; ++position) {
	    for (size_t current = 0; current < states[position].size(); ++current) {
		for (size_t previous = 0;
			previous < states[position - 1].size(); ++previous) {
		    if (costs[position - 1][previous] >= DBL_MAX)
			continue;
		    const double cost = costs[position - 1][previous] +
			states[position - 1][previous].end.DistanceTo(
			    states[position][current].start);
		    if (cost < costs[position][current]) {
			costs[position][current] = cost;
			predecessors[position][current] =
			    static_cast<int>(previous);
		    }
		}
	    }
	}

	double best_cost = DBL_MAX;
	int best_last = -1;
	for (size_t state = 0; state < states[count - 1].size(); ++state) {
	    const double cost = costs[count - 1][state] +
		states[count - 1][state].end.DistanceTo(states[0][0].start);
	    if (cost < best_cost) {
		best_cost = cost;
		best_last = static_cast<int>(state);
	    }
	}
	if (best_last < 0 || best_cost > parameter_tolerance * count ||
		!(best_cost + parameter_tolerance < original_cost))
	    continue;

	std::vector<int> selected(count, 0);
	selected[count - 1] = best_last;
	bool complete = true;
	for (size_t position = count - 1; position > 0; --position) {
	    selected[position - 1] =
		predecessors[position][selected[position]];
	    if (selected[position - 1] < 0) {
		complete = false;
		break;
	    }
	}
	if (!complete)
	    continue;

	std::vector<ShiftedCurve> candidates;
	bool exact = true;
	for (size_t position = 0; position < count; ++position) {
	    const BranchState &state = states[position][selected[position]];
	    if (state.shift[0] == 0 && state.shift[1] == 0)
		continue;
	    ON_Curve *candidate = trims[position]->DuplicateCurve();
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[0][3] = state.shift[0] * period[0];
	    translation.m_xform[1][3] = state.shift[1] * period[1];
	    std::string failure;
	    if (!candidate || !candidate->Transform(translation) ||
		    !candidate->ChangeDimension(2) || !candidate->IsValid() ||
		    !validate_periodic_trim_translation(surface, *trims[position],
			*candidate, &failure)) {
		delete candidate;
		exact = false;
		break;
	    }
	    candidates.push_back({trims[position], candidate});
	}
	if (!exact || candidates.empty()) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
		delete candidates[candidate].curve;
	    continue;
	}

	bool installed = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    const int c2_index = brep->AddTrimCurve(candidates[candidate].curve);
	    if (c2_index < 0 || !brep->SetTrimCurve(*candidates[candidate].trim,
		    c2_index)) {
		if (c2_index < 0)
		    delete candidates[candidate].curve;
		installed = false;
		break;
	    }
	    brep->SetTrimIsoFlags(*candidates[candidate].trim);
	}
	if (!installed)
	    continue;
	++repaired_loops;
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "coherently unwrapped an exact periodic pcurve loop");
    }
    return repaired_loops;
}


/* A coherent branch solve is intentionally invariant under a uniform integral
 * period translation, so it anchors the first trim rather than preferring the
 * surface's native parameter interval.  If the authored loop arrived on the
 * adjacent periodic tile, that can leave a perfectly closed loop spanning
 * [period, 2*period].  Its two seam curves are then geometrically exact but
 * OpenNURBS cannot classify the outer image as W/E (or S/N).
 *
 * Normalize only complete loops containing a reciprocal same-loop seam pair,
 * and only when every pcurve fits in one native period after the same integral
 * translation.  The candidate is transactional: every translated curve must
 * retain its complete surface lift at numerical precision, every cyclic join
 * must be no worse, every seam pair must derive complementary native-side ISO
 * flags, and directed trim validation for the face must not regress. */
size_t
normalize_exact_paired_seam_loops_to_native_domain(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    const auto complementary = [](ON_Surface::ISO first,
	    ON_Surface::ISO second) {
	return (first == ON_Surface::W_iso && second == ON_Surface::E_iso) ||
	    (first == ON_Surface::E_iso && second == ON_Surface::W_iso) ||
	    (first == ON_Surface::S_iso && second == ON_Surface::N_iso) ||
	    (first == ON_Surface::N_iso && second == ON_Surface::S_iso);
    };

    size_t normalized = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return normalized;
	const ON_BrepLoop &source_loop = brep->m_L[li];
	const ON_BrepFace *source_face = source_loop.Face();
	if (face_source_tags && (!source_face || face_source_tags->find(
		source_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *source_surface = source_face ?
	    source_face->SurfaceOf() : NULL;
	if (!source_surface || source_loop.TrimCount() < 4 ||
		(!source_surface->IsClosed(0) && !source_surface->IsClosed(1)))
	    continue;

	std::map<int, int> seam_uses;
	ON_BoundingBox loop_bounds = ON_BoundingBox::EmptyBoundingBox;
	bool complete_bounds = true;
	for (int lti = 0; lti < source_loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = source_loop.Trim(lti);
	    const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	    ON_BoundingBox curve_bounds;
	    if (!trim || !curve || !curve->GetBoundingBox(curve_bounds) ||
		    !curve_bounds.IsValid()) {
		complete_bounds = false;
		break;
	    }
	    loop_bounds.Union(curve_bounds);
	    if (trim->m_type == ON_BrepTrim::seam && trim->m_ei >= 0)
		++seam_uses[trim->m_ei];
	}
	bool has_pair = false;
	for (std::map<int, int>::const_iterator use = seam_uses.begin();
		use != seam_uses.end(); ++use)
	    has_pair = has_pair || use->second == 2;
	if (!complete_bounds || !loop_bounds.IsValid() || !has_pair)
	    continue;

	int period_shift[2] = {0, 0};
	bool shift_required = false;
	bool bounded = true;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!source_surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = source_surface->Domain(direction);
	    const double period = domain.Length();
	    const double epsilon = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		period * kPeriodicParameterSnapFraction);
	    if (!domain.IsIncreasing() || !(period > ON_ZERO_TOLERANCE)) {
		bounded = false;
		break;
	    }
	    const double minimum = loop_bounds.m_min[direction];
	    const double maximum = loop_bounds.m_max[direction];
	    const double lower = (domain.Min() - epsilon - minimum) / period;
	    const double upper = (domain.Max() + epsilon - maximum) / period;
	    if (!std::isfinite(lower) || !std::isfinite(upper)) {
		bounded = false;
		break;
	    }
	    /* Intersect in floating point before converting.  Malformed source
	     * parameters can be thousands of periods away; converting such a value
	     * to int before applying the scale bound is implementation-defined. */
	    const double bounded_lower = std::max(lower,
		-static_cast<double>(kMaximumPeriodicBranchShift));
	    const double bounded_upper = std::min(upper,
		static_cast<double>(kMaximumPeriodicBranchShift));
	    if (bounded_lower > bounded_upper) {
		bounded = false;
		break;
	    }
	    const int minimum_shift = static_cast<int>(ceil(bounded_lower));
	    const int maximum_shift = static_cast<int>(floor(bounded_upper));
	    if (minimum_shift > maximum_shift) {
		bounded = false;
		break;
	    }
	    if (minimum_shift <= 0 && maximum_shift >= 0)
		period_shift[direction] = 0;
	    else if (maximum_shift < 0)
		period_shift[direction] = maximum_shift;
	    else
		period_shift[direction] = minimum_shift;
	    shift_required = shift_required || period_shift[direction] != 0;
	}
	if (!bounded || !shift_required)
	    continue;

	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	ON_BrepLoop &candidate_loop = candidate->m_L[li];
	const ON_BrepFace *candidate_face = candidate_loop.Face();
	const ON_Surface *candidate_surface = candidate_face ?
	    candidate_face->SurfaceOf() : NULL;
	if (!candidate_surface)
	    continue;
	ON_BoundingBox surface_bounds;
	const double surface_scale = candidate_surface->GetBoundingBox(
	    surface_bounds, false) && surface_bounds.IsValid() ?
	    surface_bounds.Diagonal().Length() : 0.0;
	const double lift_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    std::max(LocalUnits::tolerance * 1.0e-8,
		surface_scale * 1.0e-12));
	ON_Xform translation(ON_Xform::IdentityTransformation);
	for (int direction = 0; direction < 2; ++direction)
	    if (candidate_surface->IsClosed(direction))
		translation.m_xform[direction][3] = period_shift[direction] *
		    candidate_surface->Domain(direction).Length();

	bool valid = true;
	for (int lti = 0; valid && lti < candidate_loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = candidate_loop.Trim(lti);
	    const ON_BrepTrim *source_trim = source_loop.Trim(lti);
	    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
	    if (!trim || !source_trim || !curve || !curve->Transform(translation) ||
		    !curve->ChangeDimension(2) || !curve->IsValid()) {
		delete curve;
		valid = false;
		break;
	    }
	    const ON_Interval source_domain = source_trim->Domain();
	    const ON_Interval candidate_domain = curve->Domain();
	    const int samples = std::min(kDenseValidationSegments,
		std::max(64, std::max(source_trim->SpanCount(),
		    curve->SpanCount()) * 8));
	    for (int sample = 0; valid && sample <= samples; ++sample) {
		const double fraction = static_cast<double>(sample) / samples;
		const ON_3dPoint source_uv = source_trim->PointAt(
		    source_domain.ParameterAt(fraction));
		const ON_3dPoint candidate_uv = curve->PointAt(
		    candidate_domain.ParameterAt(fraction));
		const ON_3dPoint source_lift = closed_surface_point_at(
		    source_surface, source_uv);
		const ON_3dPoint candidate_lift = candidate_surface->PointAt(
		    candidate_uv.x, candidate_uv.y);
		valid = source_lift.IsValid() && candidate_lift.IsValid() &&
		    source_lift.DistanceTo(candidate_lift) <= lift_tolerance;
	    }
	    ON_BoundingBox curve_bounds;
	    valid = valid && curve->GetBoundingBox(curve_bounds) &&
		curve_bounds.IsValid();
	    for (int direction = 0; valid && direction < 2; ++direction) {
		if (!candidate_surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = candidate_surface->Domain(direction);
		const double epsilon = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    domain.Length() * kPeriodicParameterSnapFraction);
		valid = curve_bounds.m_min[direction] >= domain.Min() - epsilon &&
		    curve_bounds.m_max[direction] <= domain.Max() + epsilon;
	    }
	    if (!valid) {
		delete curve;
		break;
	    }
	    const int c2_index = candidate->AddTrimCurve(curve);
	    if (c2_index < 0 || !candidate->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete curve;
		valid = false;
		break;
	    }
	    candidate->SetTrimIsoFlags(*trim);
	}

	for (int lti = 0; valid && lti < candidate_loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *source_current = source_loop.Trim(lti);
	    const ON_BrepTrim *source_next = source_loop.Trim(
		(lti + 1) % source_loop.TrimCount());
	    const ON_BrepTrim *current = candidate_loop.Trim(lti);
	    const ON_BrepTrim *next = candidate_loop.Trim(
		(lti + 1) % candidate_loop.TrimCount());
	    if (!source_current || !source_next || !current || !next) {
		valid = false;
		break;
	    }
	    const double source_gap = source_current->PointAtEnd().DistanceTo(
		source_next->PointAtStart());
	    const double candidate_gap = current->PointAtEnd().DistanceTo(
		next->PointAtStart());
	    valid = candidate_gap <= std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		source_gap + ON_ZERO_TOLERANCE * kNumericalToleranceScale);
	}
	for (std::map<int, int>::const_iterator use = seam_uses.begin();
		valid && use != seam_uses.end(); ++use) {
	    if (use->second != 2)
		continue;
	    const ON_BrepTrim *pair[2] = {NULL, NULL};
	    int pair_count = 0;
	    for (int lti = 0; lti < candidate_loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = candidate_loop.Trim(lti);
		if (trim && trim->m_type == ON_BrepTrim::seam &&
			trim->m_ei == use->first && pair_count < 2)
		    pair[pair_count++] = trim;
	    }
	    valid = pair_count == 2 && complementary(pair[0]->m_iso,
		pair[1]->m_iso);
	}
	valid = valid && !brlcad::step::FaceTrimValidationRegressed(*brep,
	    *candidate, source_face->m_face_index, LocalUnits::tolerance);
	if (!valid)
	    continue;

	*brep = *candidate;
	++normalized;
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "translated a complete paired-seam loop onto the native periodic surface domain");
    }
    return normalized;
}


/* A singular trim has no 3-D edge.  It records the otherwise collapsed
 * parameter-space side of a face at a surface pole, and its two endpoints may
 * therefore be changed independently along that exact singular boundary.
 *
 * The generic periodic branch solver above translates complete pcurves.  That
 * is correct for ordinary and seam trims, but it cannot close a loop where an
 * ordinary chain needs one integral-period shift and the singular connector
 * must keep only one of its old endpoint images.  In that case translating the
 * whole connector merely moves the same gap to its other end.
 *
 * Handle the unambiguous single-connector case transactionally.  Anchor the
 * first ordinary trim after the connector, solve integral-period images for
 * the remaining exact edge chain, and reconstruct the connector between the
 * selected endpoints.  Every translated curve is densely checked against its
 * original surface lift, every connector sample must lift to the asserted STEP
 * pole vertex, and the affected face must pass OpenNURBS validation before the
 * candidate BREP is committed. */
size_t
repair_single_singular_periodic_loop_branches(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool record_repair, const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	const ON_BrepLoop &source_loop = brep->m_L[li];
	const ON_BrepFace *source_face = source_loop.Face();
	if (face_source_tags && (!source_face || face_source_tags->find(
		source_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *source_surface = source_face ?
	    source_face->SurfaceOf() : NULL;
	if (!source_surface || source_loop.TrimCount() < 3 ||
		(!source_surface->IsClosed(0) &&
		 !source_surface->IsClosed(1)))
	    continue;

	int singular_position = -1;
	int singular_count = 0;
	for (int lti = 0; lti < source_loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = source_loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::singular) {
		singular_position = lti;
		++singular_count;
	    }
	}
	if (singular_count != 1)
	    continue;
	const ON_BrepTrim *source_singular =
	    source_loop.Trim(singular_position);
	if (!source_singular || source_singular->m_ei >= 0 ||
		source_singular->m_vi[0] < 0 ||
		source_singular->m_vi[0] != source_singular->m_vi[1] ||
		source_singular->m_vi[0] >= brep->m_V.Count())
	    continue;

	int direction = -1;
	switch (source_singular->m_iso) {
	    case ON_Surface::S_iso:
	    case ON_Surface::N_iso:
		direction = 0;
		break;
	    case ON_Surface::E_iso:
	    case ON_Surface::W_iso:
		direction = 1;
		break;
	    default:
		break;
	}
	if (direction < 0 || !source_surface->IsClosed(direction))
	    continue;
	const double period = source_surface->Domain(direction).Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;
	const double parameter_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * kPeriodicParameterSnapFraction);

	std::vector<int> ordinary_positions;
	for (int offset = 1; offset < source_loop.TrimCount(); ++offset) {
	    const int position =
		(singular_position + offset) % source_loop.TrimCount();
	    const ON_BrepTrim *trim = source_loop.Trim(position);
	    if (!trim || trim->m_type == ON_BrepTrim::singular ||
		    !trim->Edge()) {
		ordinary_positions.clear();
		break;
	    }
	    ordinary_positions.push_back(position);
	}
	if (ordinary_positions.size() < 2)
	    continue;
	const ON_BrepTrim *first_ordinary =
	    source_loop.Trim(ordinary_positions.front());
	const ON_BrepTrim *last_ordinary =
	    source_loop.Trim(ordinary_positions.back());
	if (!first_ordinary || !last_ordinary ||
		source_singular->m_vi[1] != first_ordinary->m_vi[0] ||
		last_ordinary->m_vi[1] != source_singular->m_vi[0])
	    continue;

	/* Prove the immutable STEP topology and model-space coincidence of every
	 * ordinary join before periodicity is allowed to alter any pcurve. */
	double original_cost = 0.0;
	bool exact_chain = true;
	for (size_t position = 1; position < ordinary_positions.size();
		++position) {
	    const ON_BrepTrim *previous =
		source_loop.Trim(ordinary_positions[position - 1]);
	    const ON_BrepTrim *next =
		source_loop.Trim(ordinary_positions[position]);
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count()) {
		exact_chain = false;
		break;
	    }
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = source_surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = source_surface->PointAt(
		next_uv.x, next_uv.y);
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge())
		tolerance = std::max(tolerance,
		    previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const ON_3dPoint &vertex =
		brep->m_V[previous->m_vi[1]].point;
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > tolerance ||
		    next_lift.DistanceTo(vertex) > tolerance) {
		exact_chain = false;
		break;
	    }
	    original_cost += previous_uv.DistanceTo(next_uv);
	}
	if (!exact_chain ||
		original_cost <= parameter_tolerance *
		    static_cast<double>(ordinary_positions.size() - 1))
	    continue;

	struct BranchState {
	    int shift;
	    ON_3dPoint start;
	    ON_3dPoint end;
	};
	std::vector<std::vector<BranchState> > states(
	    ordinary_positions.size());
	for (size_t position = 0; position < ordinary_positions.size();
		++position) {
	    const ON_BrepTrim *trim =
		source_loop.Trim(ordinary_positions[position]);
	    const int minimum = position == 0 ? 0 :
		-kMaximumPeriodicBranchShift;
	    const int maximum = position == 0 ? 0 :
		kMaximumPeriodicBranchShift;
	    for (int shift = minimum; shift <= maximum; ++shift) {
		BranchState state;
		state.shift = shift;
		state.start = trim->PointAtStart();
		state.end = trim->PointAtEnd();
		state.start[direction] += shift * period;
		state.end[direction] += shift * period;
		states[position].push_back(state);
	    }
	}

	std::vector<std::vector<double> > costs(ordinary_positions.size());
	std::vector<std::vector<int> > predecessors(ordinary_positions.size());
	for (size_t position = 0; position < ordinary_positions.size();
		++position) {
	    costs[position].assign(states[position].size(), DBL_MAX);
	    predecessors[position].assign(states[position].size(), -1);
	}
	costs[0][0] = 0.0;
	for (size_t position = 1; position < ordinary_positions.size();
		++position) {
	    for (size_t current = 0; current < states[position].size();
		    ++current) {
		for (size_t previous = 0;
			previous < states[position - 1].size(); ++previous) {
		    if (costs[position - 1][previous] >= DBL_MAX)
			continue;
		    const double cost = costs[position - 1][previous] +
			states[position - 1][previous].end.DistanceTo(
			    states[position][current].start);
		    if (cost < costs[position][current]) {
			costs[position][current] = cost;
			predecessors[position][current] =
			    static_cast<int>(previous);
		    }
		}
	    }
	}

	double best_cost = DBL_MAX;
	int best_last = -1;
	const size_t last_position = ordinary_positions.size() - 1;
	for (size_t state = 0; state < states[last_position].size(); ++state) {
	    if (costs[last_position][state] < best_cost) {
		best_cost = costs[last_position][state];
		best_last = static_cast<int>(state);
	    }
	}
	if (best_last < 0 ||
		best_cost > parameter_tolerance *
		    static_cast<double>(ordinary_positions.size() - 1) ||
		!(best_cost + parameter_tolerance < original_cost))
	    continue;

	std::vector<int> selected(ordinary_positions.size(), 0);
	selected[last_position] = best_last;
	bool complete = true;
	for (size_t position = last_position; position > 0; --position) {
	    selected[position - 1] =
		predecessors[position][selected[position]];
	    if (selected[position - 1] < 0) {
		complete = false;
		break;
	    }
	}
	if (!complete)
	    continue;

	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	ON_BrepLoop &candidate_loop = candidate->m_L[li];
	const ON_BrepFace *candidate_face = candidate_loop.Face();
	const ON_Surface *candidate_surface = candidate_face ?
	    candidate_face->SurfaceOf() : NULL;
	if (!candidate_surface)
	    continue;
	bool installed = true;
	bool shifted = false;
	for (size_t position = 0; installed &&
		position < ordinary_positions.size(); ++position) {
	    const BranchState &state = states[position][selected[position]];
	    if (!state.shift)
		continue;
	    ON_BrepTrim *trim =
		candidate_loop.Trim(ordinary_positions[position]);
	    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[direction][3] = state.shift * period;
	    std::string failure;
	    if (!trim || !curve || !curve->Transform(translation) ||
		    !curve->ChangeDimension(2) || !curve->IsValid() ||
		    !validate_periodic_trim_translation(candidate_surface,
			*trim, *curve, &failure)) {
		delete curve;
		installed = false;
		break;
	    }
	    const int c2_index = candidate->AddTrimCurve(curve);
	    if (c2_index < 0 || !candidate->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete curve;
		installed = false;
		break;
	    }
	    candidate->SetTrimIsoFlags(*trim);
	    shifted = true;
	}
	if (!installed || !shifted)
	    continue;

	ON_BrepTrim *candidate_singular =
	    candidate_loop.Trim(singular_position);
	ON_BrepTrim *candidate_first =
	    candidate_loop.Trim(ordinary_positions.front());
	ON_BrepTrim *candidate_last =
	    candidate_loop.Trim(ordinary_positions.back());
	if (!candidate_singular || !candidate_first || !candidate_last)
	    continue;
	const ON_3dPoint connector_start = candidate_last->PointAtEnd();
	const ON_3dPoint connector_end = candidate_first->PointAtStart();
	std::unique_ptr<ON_LineCurve> connector(
	    new ON_LineCurve(connector_start, connector_end));
	if (!connector->ChangeDimension(2) || !connector->IsValid())
	    continue;

	const ON_3dPoint &pole =
	    candidate->m_V[candidate_singular->m_vi[0]].point;
	double pole_tolerance = std::max(LocalUnits::tolerance,
	    candidate->m_V[candidate_singular->m_vi[0]].m_tolerance);
	pole_tolerance = std::max(pole_tolerance,
	    std::max(candidate_singular->m_tolerance[0],
		candidate_singular->m_tolerance[1]));
	if (candidate_first->Edge())
	    pole_tolerance = std::max(pole_tolerance,
		candidate_first->Edge()->m_tolerance);
	if (candidate_last->Edge())
	    pole_tolerance = std::max(pole_tolerance,
		candidate_last->Edge()->m_tolerance);
	bool exact_connector = true;
	for (int sample = 0;
		sample <= kPcurveLocusScreeningSegments; ++sample) {
	    const ON_3dPoint uv = connector->PointAt(
		connector->Domain().ParameterAt(
		    static_cast<double>(sample) /
		    kPcurveLocusScreeningSegments));
	    const ON_3dPoint lift = candidate_surface->PointAt(uv.x, uv.y);
	    if (!lift.IsValid() || lift.DistanceTo(pole) > pole_tolerance) {
		exact_connector = false;
		break;
	    }
	}
	if (!exact_connector)
	    continue;

	const ON_Surface::ISO singular_iso = candidate_singular->m_iso;
	const int connector_index =
	    candidate->AddTrimCurve(connector.release());
	if (connector_index < 0 ||
		!candidate->SetTrimCurve(*candidate_singular,
		    connector_index))
	    continue;
	candidate_singular->m_iso = singular_iso;
	candidate_singular->m_tolerance[0] = std::max(
	    candidate_singular->m_tolerance[0], pole_tolerance);
	candidate_singular->m_tolerance[1] = std::max(
	    candidate_singular->m_tolerance[1], pole_tolerance);

	ON_wString loop_messages;
	ON_TextLog loop_log(loop_messages);
	ON_wString face_messages;
	ON_TextLog face_log(face_messages);
	if (!candidate_loop.IsValid(&loop_log) ||
		!candidate_face->IsValid(&face_log))
	    continue;

	*brep = *candidate;
	++repaired_loops;
	if (record_repair)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"reclosed an exact periodic edge chain around one singular pole connector");
    }
    return repaired_loops;
}


/* Multiple pole connectors divide a periodic loop into independent ordinary
 * edge subchains.  A whole-loop branch solver cannot repair one such subchain:
 * translating everything merely moves its integral-period gap to the next
 * connector.  Solve each ordinary subchain with its first trim fixed, translate
 * only later trims whose shared STEP vertex proves one unique periodic image,
 * then reconstruct (or remove) the flexible singular connectors.  The complete
 * affected face is validated on one BREP copy before any source topology is
 * changed. */
size_t
repair_multi_singular_periodic_loop_branches(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool record_repair, const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	const ON_BrepLoop &source_loop = brep->m_L[li];
	const ON_BrepFace *source_face = source_loop.Face();
	if (face_source_tags && (!source_face || face_source_tags->find(
		source_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *source_surface = source_face ?
	    source_face->SurfaceOf() : NULL;
	const int trim_count = source_loop.TrimCount();
	if (!source_surface || trim_count < 4)
	    continue;

	std::vector<int> singular_positions;
	int direction = -1;
	bool compatible_singulars = true;
	for (int lti = 0; lti < trim_count; ++lti) {
	    const ON_BrepTrim *trim = source_loop.Trim(lti);
	    if (!trim || trim->m_type != ON_BrepTrim::singular)
		continue;
	    int singular_direction = -1;
	    if (trim->m_iso == ON_Surface::S_iso ||
		    trim->m_iso == ON_Surface::N_iso)
		singular_direction = 0;
	    else if (trim->m_iso == ON_Surface::W_iso ||
		    trim->m_iso == ON_Surface::E_iso)
		singular_direction = 1;
	    if (singular_direction < 0 ||
		    (direction >= 0 && direction != singular_direction)) {
		compatible_singulars = false;
		break;
	    }
	    direction = singular_direction;
	    singular_positions.push_back(lti);
	}
	if (!compatible_singulars || singular_positions.size() < 2 ||
		direction < 0 || !source_surface->IsClosed(direction))
	    continue;
	const double period = source_surface->Domain(direction).Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;
	const double parameter_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * kPeriodicParameterSnapFraction);

	/* Map loop positions to the integral-period translations selected by
	 * topology.  Every ordinary run starts immediately after a singular trim;
	 * its first curve is an arbitrary but fixed branch anchor. */
	std::map<int, int> selected_shifts;
	std::map<int, ON_3dPoint> constrained_starts;
	std::map<int, double> constrained_tolerances;
	bool exact = true;
	for (size_t singular = 0; exact &&
		singular < singular_positions.size(); ++singular) {
	    const int run_start = (singular_positions[singular] + 1) % trim_count;
	    const int run_end = singular_positions[
		(singular + 1) % singular_positions.size()];
	    int previous_position = -1;
	    int position = run_start;
	    while (position != run_end) {
		const ON_BrepTrim *trim = source_loop.Trim(position);
		if (!trim || trim->m_type == ON_BrepTrim::singular ||
			!trim->Edge()) {
		    exact = false;
		    break;
		}
		if (previous_position < 0) {
		    selected_shifts[position] = 0;
		    previous_position = position;
		    position = (position + 1) % trim_count;
		    continue;
		}
		const ON_BrepTrim *previous =
		    source_loop.Trim(previous_position);
		if (!previous || previous->m_vi[1] < 0 ||
			previous->m_vi[1] != trim->m_vi[0] ||
			previous->m_vi[1] >= brep->m_V.Count()) {
		    exact = false;
		    break;
		}
		ON_3dPoint previous_end = previous->PointAtEnd();
		previous_end[direction] +=
		    selected_shifts[previous_position] * period;
		const ON_3dPoint trim_start = trim->PointAtStart();
		const int shift = static_cast<int>(llround(
		    (previous_end[direction] - trim_start[direction]) / period));
		ON_3dPoint shifted_start = trim_start;
		shifted_start[direction] += shift * period;
		const ON_3dPoint previous_lift = source_surface->PointAt(
		    previous_end.x, previous_end.y);
		const ON_3dPoint shifted_lift = source_surface->PointAt(
		    shifted_start.x, shifted_start.y);
		const ON_3dPoint &vertex =
		    brep->m_V[previous->m_vi[1]].point;
		double tolerance = LocalUnits::tolerance;
		if (previous->Edge())
		    tolerance = std::max(tolerance,
			previous->Edge()->m_tolerance);
		tolerance = std::max(tolerance, trim->Edge()->m_tolerance);
		tolerance = std::max(tolerance,
		    brep->m_V[previous->m_vi[1]].m_tolerance);
		const int open_direction = 1 - direction;
		const double open_span =
		    source_surface->Domain(open_direction).Length();
		const double closed_residual = fabs(
		    previous_end[direction] - shifted_start[direction]);
		const double open_residual = fabs(
		    previous_end[open_direction] -
		    shifted_start[open_direction]);
		if (std::abs(shift) > kMaximumPeriodicBranchShift ||
			closed_residual > parameter_tolerance ||
			!(open_span > ON_ZERO_TOLERANCE) ||
			open_residual >= 0.5 * open_span ||
			!previous_lift.IsValid() || !shifted_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > tolerance ||
			shifted_lift.DistanceTo(vertex) > tolerance) {
		    exact = false;
		    break;
		}
		selected_shifts[position] = shift;
		if (open_residual > parameter_tolerance)
		    constrained_starts[position] = previous_end;
		if (open_residual > parameter_tolerance)
		    constrained_tolerances[position] = tolerance;
		previous_position = position;
		position = (position + 1) % trim_count;
	    }
	    if (run_start == run_end)
		exact = false;
	}
	bool changed = false;
	for (std::map<int, int>::const_iterator shift = selected_shifts.begin();
		shift != selected_shifts.end(); ++shift)
	    changed = changed || shift->second != 0;
	if (!exact || !changed)
	    continue;

	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	ON_BrepLoop *candidate_loop = li >= 0 && li < candidate->m_L.Count() ?
	    &candidate->m_L[li] : NULL;
	const ON_BrepFace *candidate_face = candidate_loop ?
	    candidate_loop->Face() : NULL;
	const ON_Surface *candidate_surface = candidate_face ?
	    candidate_face->SurfaceOf() : NULL;
	if (!candidate_loop || !candidate_surface)
	    continue;

	for (std::map<int, int>::const_iterator shift = selected_shifts.begin();
		exact && shift != selected_shifts.end(); ++shift) {
	    if (!shift->second)
		continue;
	    ON_BrepTrim *trim = candidate_loop->Trim(shift->first);
	    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[direction][3] = shift->second * period;
	    std::string failure;
	    const std::map<int, ON_3dPoint>::const_iterator constrained =
		constrained_starts.find(shift->first);
	    const std::map<int, double>::const_iterator constrained_tolerance =
		constrained_tolerances.find(shift->first);
	    double measured_bounded_mismatch = 0.0;
	    if (!trim || !curve || !curve->Transform(translation) ||
		    (constrained != constrained_starts.end() &&
		     !curve->SetStartPoint(constrained->second)) ||
		    !curve->ChangeDimension(2) || !curve->IsValid() ||
		    !validate_periodic_trim_translation(candidate_surface, *trim,
			*curve, &failure, constrained_tolerance !=
			    constrained_tolerances.end() ?
			    constrained_tolerance->second : -1.0,
			&measured_bounded_mismatch)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": multi-singular periodic branch candidate L" << li
			<< "/T" << (trim ? trim->m_trim_index : -1)
			<< " shift " << shift->second << " rejected"
			<< (constrained != constrained_starts.end() ?
			    " after measured endpoint constraint" : "")
			<< (failure.empty() ? "" : ": " + failure) << std::endl;
		delete curve;
		exact = false;
		break;
	    }
	    if (constrained_tolerance != constrained_tolerances.end() &&
		    measured_bounded_mismatch > LocalUnits::tolerance) {
		trim->m_tolerance[0] = std::max(trim->m_tolerance[0],
		    measured_bounded_mismatch * kRegenerationToleranceSafety);
		trim->m_tolerance[1] = std::max(trim->m_tolerance[1],
		    measured_bounded_mismatch * kRegenerationToleranceSafety);
	    }
	    const int c2_index = candidate->AddTrimCurve(curve);
	    if (c2_index < 0 || !candidate->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete curve;
		exact = false;
		break;
	    }
	    candidate->SetTrimIsoFlags(*trim);
	}
	if (!exact)
	    continue;

	std::vector<int> redundant_singular_trims;
	for (std::vector<int>::const_iterator singular =
		singular_positions.begin(); exact &&
		singular != singular_positions.end(); ++singular) {
	    ON_BrepTrim *connector = candidate_loop->Trim(*singular);
	    ON_BrepTrim *previous = candidate_loop->Trim(
		(*singular + trim_count - 1) % trim_count);
	    ON_BrepTrim *next =
		candidate_loop->Trim((*singular + 1) % trim_count);
	    if (!connector || !previous || !next ||
		    connector->m_vi[0] < 0 ||
		    connector->m_vi[0] != connector->m_vi[1] ||
		    connector->m_vi[0] >= candidate->m_V.Count()) {
		exact = false;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": multi-singular periodic branch candidate L" << li
			<< " had invalid connector topology at position "
			<< *singular << std::endl;
		break;
	    }
	    const ON_3dPoint connector_start = previous->PointAtEnd();
	    const ON_3dPoint connector_end = next->PointAtStart();
	    const ON_3dPoint &pole =
		candidate->m_V[connector->m_vi[0]].point;
	    double tolerance = std::max(LocalUnits::tolerance,
		candidate->m_V[connector->m_vi[0]].m_tolerance);
	    tolerance = std::max(tolerance,
		std::max(connector->m_tolerance[0],
		    connector->m_tolerance[1]));
	    if (previous->Edge())
		tolerance = std::max(tolerance,
		    previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    if (connector_start.DistanceTo(connector_end) <=
		    parameter_tolerance) {
		redundant_singular_trims.push_back(
		    connector->m_trim_index);
		continue;
	    }
	    std::unique_ptr<ON_LineCurve> replacement(
		new ON_LineCurve(connector_start, connector_end));
	    if (!replacement->ChangeDimension(2) ||
		    !replacement->IsValid()) {
		exact = false;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": multi-singular periodic branch candidate L" << li
			<< " could not rebuild connector T"
			<< connector->m_trim_index << std::endl;
		break;
	    }
	    for (int sample = 0; exact &&
		    sample <= kPcurveLocusScreeningSegments; ++sample) {
		const ON_3dPoint uv = replacement->PointAt(
		    replacement->Domain().ParameterAt(
			static_cast<double>(sample) /
			kPcurveLocusScreeningSegments));
		const ON_3dPoint lift =
		    candidate_surface->PointAt(uv.x, uv.y);
		exact = lift.IsValid() &&
		    lift.DistanceTo(pole) <= tolerance;
	    }
	    if (!exact)
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": multi-singular periodic branch candidate L" << li
			<< " connector T" << connector->m_trim_index
			<< " left its asserted pole tolerance " << tolerance
			<< std::endl;
	    if (!exact)
		break;
	    const ON_Surface::ISO original_iso = connector->m_iso;
	    const int c2_index =
		candidate->AddTrimCurve(replacement.release());
	    if (c2_index < 0 ||
		    !candidate->SetTrimCurve(*connector, c2_index)) {
		exact = false;
		break;
	    }
	    connector->m_iso = original_iso;
	    connector->m_tolerance[0] =
		std::max(connector->m_tolerance[0], tolerance);
	    connector->m_tolerance[1] =
		std::max(connector->m_tolerance[1], tolerance);
	}
	if (!exact)
	    continue;

	std::sort(redundant_singular_trims.begin(),
	    redundant_singular_trims.end(), std::greater<int>());
	for (std::vector<int>::const_iterator trim =
		redundant_singular_trims.begin();
		exact && trim != redundant_singular_trims.end(); ++trim) {
	    if (*trim < 0 || *trim >= candidate->m_T.Count()) {
		exact = false;
		break;
	    }
	    candidate->DeleteTrim(candidate->m_T[*trim], true);
	}
	if (!exact || (!redundant_singular_trims.empty() &&
		!candidate->Compact()))
	    continue;

	/* Compaction can renumber topology.  Recover the affected source face and
	 * loop by their immutable STEP tags before the transactional validity
	 * check. */
	const int source_face_tag = source_face->m_face_user.i;
	const int source_loop_tag = source_loop.m_loop_user.i;
	const ON_BrepLoop *validated_loop = NULL;
	const ON_BrepFace *validated_face = NULL;
	for (int candidate_li = 0; candidate_li < candidate->m_L.Count();
		++candidate_li) {
	    const ON_BrepLoop &probe = candidate->m_L[candidate_li];
	    const ON_BrepFace *probe_face = probe.Face();
	    if (probe.m_loop_user.i == source_loop_tag && probe_face &&
		    probe_face->m_face_user.i == source_face_tag) {
		validated_loop = &probe;
		validated_face = probe_face;
		break;
	    }
	}
	ON_wString loop_messages;
	ON_TextLog loop_log(loop_messages);
	ON_wString face_messages;
	ON_TextLog face_log(face_messages);
	if (!validated_loop || !validated_face ||
		!validated_loop->IsValid(&loop_log) ||
		!validated_face->IsValid(&face_log)) {
	    if (wrapper->Verbose()) {
		ON_String loop_text(loop_messages);
		ON_String face_text(face_messages);
		std::cerr << entity_type << " #" << entity_id
		    << ": multi-singular periodic branch candidate L" << li
		    << " failed transactional validation:\n"
		    << loop_text.Array() << face_text.Array();
	    }
	    continue;
	}

	*brep = *candidate;
	++repaired_loops;
	if (record_repair)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"reclosed exact periodic edge subchains between singular pole connectors");
    }
    return repaired_loops;
}


size_t
regenerate_periodic_loop_chains(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, int only_loop,
	bool record_repair, bool prefer_cyclic_start_image)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (only_loop >= 0 && li != only_loop)
	    continue;
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 2 ||
		(!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	bool has_paired_edge_use = false;
	std::set<int> loop_edges;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_ei >= 0 &&
		    !loop_edges.insert(trim->m_ei).second)
		has_paired_edge_use = true;
	}

	/* A very large raw UV discontinuity can be a succession of equivalent
	 * periodic branch choices rather than a 3-D topology gap.  Rebuild only
	 * loops for which the two source pcurve lifts still meet the asserted STEP
	 * vertex within the already proven edge/vertex tolerances. */
	bool needs_regeneration = false;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count())
		continue;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    bool large_periodic_gap = false;
	    for (int direction = 0; direction < 2; ++direction) {
		const double period = surface->Domain(direction).Length();
		/* One period is an ordinary min/max discontinuity only at an explicit
		 * seam.  Two ordinary boundary trims one period apart leave an invalid
		 * p-space loop and require coherent whole-loop regeneration.  A paired
		 * edge use in this same loop is OpenNURBS' topology definition of a seam,
		 * including an interior keyhole/slit.  Such a loop must also close
		 * numerically at each join, even when an earlier derived m_iso happened
		 * to classify the pair as a surface-boundary seam. */
		const bool ordinary_join = previous->m_type != ON_BrepTrim::seam &&
		    next->m_type != ON_BrepTrim::seam &&
		    previous->m_type != ON_BrepTrim::singular &&
		    next->m_type != ON_BrepTrim::singular;
		const double gap_threshold = ordinary_join || has_paired_edge_use ?
		    0.5 : 1.5;
		if (surface->IsClosed(direction) && period > ON_ZERO_TOLERANCE &&
			fabs(previous_uv[direction] - next_uv[direction]) >
			    gap_threshold * period) {
		    large_periodic_gap = true;
		    break;
		}
	    }
	    if (!large_periodic_gap)
		continue;
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge()) tolerance = std::max(tolerance,
		previous->Edge()->m_tolerance);
	    if (next->Edge()) tolerance = std::max(tolerance,
		next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    if (previous_lift.IsValid() && next_lift.IsValid() &&
		    previous_lift.DistanceTo(vertex) <= tolerance &&
		    next_lift.DistanceTo(vertex) <= tolerance) {
		needs_regeneration = true;
		break;
	    }
	}
	if (!needs_regeneration)
	    continue;

	struct OriginalTrimCurve {
	    int trim_index;
	    ON_Curve *curve;
	    ON_Surface::ISO iso;
	};
	std::vector<OriginalTrimCurve> originals;
	originals.reserve(loop.TrimCount());
	bool saved = true;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
	    if (!trim || !curve) {
		delete curve;
		saved = false;
		break;
	    }
	    originals.push_back({trim->m_trim_index, curve, trim->m_iso});
	}
	if (!saved) {
	    for (std::vector<OriginalTrimCurve>::iterator original =
		    originals.begin(); original != originals.end(); ++original)
		delete original->curve;
	    continue;
	}

	ON_BrepTrim *first = loop.Trim(0);
	ON_BrepEdge *first_edge = first ? first->Edge() : NULL;
	double first_tolerance = LocalUnits::tolerance;
	if (first_edge)
	    first_tolerance = std::max(first_tolerance, first_edge->m_tolerance);
	ON_3dPoint loop_start = first ? first->PointAtStart() :
	    ON_3dPoint::UnsetPoint;
	if (first && first_edge && first->m_vi[0] >= 0 &&
		first->m_vi[0] < brep->m_V.Count())
	    first_tolerance = std::max(first_tolerance,
		brep->m_V[first->m_vi[0]].m_tolerance);
	if (first && first_edge)
	    normalize_closed_surface_parameter(surface, first_edge->PointAt(
		first_edge->Domain()[first->m_bRev3d ? 1 : 0]),
		first_tolerance, loop_start);
	if (prefer_cyclic_start_image && first && first_edge &&
		loop_start.IsValid()) {
	    const ON_BrepTrim *last = loop.Trim(loop.TrimCount() - 1);
	    const ON_3dPoint last_end = last ? last->PointAtEnd() :
		ON_3dPoint::UnsetPoint;
	    const ON_3dPoint edge_start = first_edge->PointAt(
		first_edge->Domain()[first->m_bRev3d ? 1 : 0]);
	    if (last_end.IsValid() && edge_start.IsValid()) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted = loop_start;
		    shifted[direction] += round((last_end[direction] -
			shifted[direction]) / period) * period;
		    const ON_3dPoint shifted_lift =
			closed_surface_point_at(surface, shifted);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_start) <=
				first_tolerance)
			loop_start = shifted;
		}
	    }
	}

	bool regenerated = first && first_edge && loop_start.IsValid();
	ON_3dPoint required_start = loop_start;
	std::string failure;
	for (int lti = 0; regenerated && lti < loop.TrimCount(); ++lti) {
	    if (brlcad::PullbackWorkCancelled()) {
		regenerated = false;
		failure = "periodic loop-chain regeneration was cancelled";
		break;
	    }
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    ON_NurbsCurve edge_nurbs;
	    double tolerance = LocalUnits::tolerance;
	    if (edge)
		tolerance = std::max(tolerance, edge->m_tolerance);
	    if (trim && trim->m_vi[0] >= 0 && trim->m_vi[0] < brep->m_V.Count())
		tolerance = std::max(tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
	    if (trim && trim->m_vi[1] >= 0 && trim->m_vi[1] < brep->m_V.Count())
		tolerance = std::max(tolerance,
		    brep->m_V[trim->m_vi[1]].m_tolerance);
	    if (trim && edge && edge->GetNurbForm(edge_nurbs))
		tolerance = verified_regeneration_tolerance(*trim, *edge,
		    surface, edge_nurbs, tolerance, brep, wrapper, entity_id,
		    entity_type);
	    const ON_3dPoint *required_end = lti + 1 == loop.TrimCount() ?
		&loop_start : NULL;
	    regenerated = trim && edge && edge_nurbs.IsValid() &&
		regenerate_trim_polyline(brep, *trim, surface, edge_nurbs,
		    tolerance, &failure, NULL, &required_start, required_end, true,
		    wrapper, has_paired_edge_use || prefer_cyclic_start_image);
	    if (regenerated)
		required_start = trim->PointAtEnd();
	}
	bool needs_endpoint_reconciliation = false;
	for (int lti = 0; regenerated && lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    const double gap = previous && next ? previous->PointAtEnd().DistanceTo(
		next->PointAtStart()) : DBL_MAX;
	    if (previous && next && gap <= ON_ZERO_TOLERANCE)
		continue;
	    if (prefer_cyclic_start_image && wrapper && wrapper->Verbose() &&
		    previous && next) {
		const ON_3dPoint previous_end = previous->PointAtEnd();
		const ON_3dPoint next_start = next->PointAtStart();
		const ON_BrepEdge *previous_edge = previous->Edge();
		const ON_BrepEdge *next_edge = next->Edge();
		std::cerr << entity_type << " #" << entity_id << ": loop " << li
		    << " regenerated cyclic join " << lti << '/'
		    << (lti + 1) % loop.TrimCount() << " remained open by "
		    << gap << " at " << previous_end.x << ':' << previous_end.y
		    << " -> " << next_start.x << ':' << next_start.y
		    << " (STEP edges "
		    << (previous_edge ? previous_edge->m_edge_user.i : 0) << '/'
		    << (next_edge ? next_edge->m_edge_user.i : 0) << ')'
		    << std::endl;
	    }
	    bool topology_proves_join = false;
	    if (previous && next && previous->m_vi[1] >= 0 &&
		    previous->m_vi[1] == next->m_vi[0] &&
		    previous->m_vi[1] < brep->m_V.Count()) {
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		double join_tolerance = LocalUnits::tolerance;
		if (previous->Edge()) join_tolerance = std::max(join_tolerance,
		    previous->Edge()->m_tolerance);
		if (next->Edge()) join_tolerance = std::max(join_tolerance,
		    next->Edge()->m_tolerance);
		join_tolerance = std::max(join_tolerance,
		    brep->m_V[previous->m_vi[1]].m_tolerance);
		const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
		topology_proves_join = previous_lift.IsValid() &&
		    next_lift.IsValid() && previous_lift.DistanceTo(vertex) <=
			join_tolerance && next_lift.DistanceTo(vertex) <=
			join_tolerance && previous_lift.DistanceTo(next_lift) <=
			join_tolerance;
	    }
	    if (!topology_proves_join) {
		regenerated = false;
		failure = "regenerated join " + std::to_string(lti) + "/" +
		    std::to_string((lti + 1) % loop.TrimCount()) +
		    " retained an unproven parameter-space gap of " +
		    std::to_string(gap);
		break;
	    }
	    needs_endpoint_reconciliation = true;
	}

	/* Regeneration is intentionally transactional.  The immutable STEP edges
	 * establish the candidate pcurves, but their independently solved periodic
	 * branches can retain a small UV join gap even when both lifts meet the
	 * same exact topology vertex.  Run the ordinary bounded endpoint repair on
	 * a copy of this BREP and commit only if that one loop becomes exactly
	 * closed and every directed trim remains valid.  This makes the existing
	 * dense edge/surface checks authoritative instead of widening a
	 * parameter-space magic threshold. */
	if (regenerated && needs_endpoint_reconciliation) {
	    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	    repair_adjacent_trim_endpoints(candidate.get(), wrapper, entity_id,
		entity_type, li, false);
	    bool candidate_valid = li >= 0 && li < candidate->m_L.Count();
	    ON_BrepLoop *candidate_loop = candidate_valid ?
		&candidate->m_L[li] : NULL;
	    for (int lti = 0; candidate_valid &&
		    lti < candidate_loop->TrimCount(); ++lti) {
		const ON_BrepTrim *previous = candidate_loop->Trim(lti);
		const ON_BrepTrim *next = candidate_loop->Trim(
		    (lti + 1) % candidate_loop->TrimCount());
		const double gap = previous && next ?
		    previous->PointAtEnd().DistanceTo(next->PointAtStart()) :
		    DBL_MAX;
		if (!previous || !next || gap > ON_ZERO_TOLERANCE) {
		    candidate_valid = false;
		    failure = "bounded endpoint reconciliation left join " +
			std::to_string(lti) + "/" +
			std::to_string((lti + 1) %
			    candidate_loop->TrimCount()) + " open by " +
			std::to_string(gap);
		    break;
		}
		ON_wString trim_messages;
		ON_TextLog trim_log(trim_messages);
		if (!previous->IsValid(&trim_log)) {
		    candidate_valid = false;
		    failure = "bounded endpoint reconciliation invalidated trim " +
			std::to_string(previous->m_trim_index);
		}
	    }
	    if (candidate_valid) {
		*brep = *candidate;
		if (record_repair)
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"closed residual joins on an exact regenerated periodic loop chain");
	    } else {
		regenerated = false;
	    }
	}
	/* UV closure and surface lift are necessary but do not by themselves prove
	 * that each regenerated pcurve retains the trim's directed 3-D edge
	 * correspondence.  In particular, a loop reconstructed after a periodic
	 * band reversal can close perfectly while one endpoint is attached to the
	 * opposite end of its open edge.  Keep this loop-level operation
	 * transactional by running OpenNURBS' trim invariant before releasing the
	 * saved curves. */
	ON_BrepLoop *validated_loop = regenerated && li >= 0 &&
	    li < brep->m_L.Count() ? &brep->m_L[li] : NULL;
	for (int lti = 0; regenerated && validated_loop &&
		lti < validated_loop->TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = validated_loop->Trim(lti);
	    ON_wString trim_messages;
	    ON_TextLog trim_log(trim_messages);
	    if (!trim || !trim->IsValid(&trim_log)) {
		regenerated = false;
		failure = "regenerated trim " + std::to_string(
		    trim ? trim->m_trim_index : -1) +
		    " failed directed edge validation";
	    }
	}

	if (!regenerated) {
	    for (std::vector<OriginalTrimCurve>::iterator original =
		    originals.begin(); original != originals.end(); ++original) {
		const int c2_index = brep->AddTrimCurve(original->curve);
		if (c2_index >= 0) {
		    original->curve = NULL;
		    if (original->trim_index >= 0 &&
			    original->trim_index < brep->m_T.Count() &&
			    brep->SetTrimCurve(brep->m_T[original->trim_index],
				c2_index))
			brep->m_T[original->trim_index].m_iso = original->iso;
		}
		delete original->curve;
	    }
	    if (wrapper->Verbose() && !failure.empty())
		std::cerr << entity_type << " #" << entity_id << ": loop " << li
		    << " periodic chain regeneration rejected: " << failure
		    << std::endl;
	    continue;
	}
	for (std::vector<OriginalTrimCurve>::iterator original = originals.begin();
		original != originals.end(); ++original)
	    delete original->curve;
	++repaired_loops;
	if (record_repair)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"regenerated an exact periodic loop chain from its 3-D STEP edges");
    }
    return repaired_loops;
}


void
repair_adjacent_trim_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, int only_loop,
	bool record_changes, bool skip_closing_join,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;
    if (only_loop >= brep->m_L.Count())
	return;
    const auto record_repair = [wrapper, entity_id, &entity_type,
	    record_changes](const char *attribute, const char *message) {
	if (record_changes)
	    wrapper->RecordRepair(entity_id, entity_type, attribute, message);
    };

    const auto boundary_axis = [](ON_Surface::ISO iso, int axis) {
	if (iso <= ON_Surface::not_iso)
	    return false;
	return axis == 0 ? (static_cast<int>(iso) % 2 == 1) :
	    (static_cast<int>(iso) % 2 == 0);
    };
    const auto boundary_parameter = [](const ON_Surface *surface,
	    ON_Surface::ISO iso, int *axis, double *parameter) {
	if (!surface || !axis || !parameter)
	    return false;
	switch (iso) {
	    case ON_Surface::W_iso:
		*axis = 0;
		*parameter = surface->Domain(0).Min();
		return true;
	    case ON_Surface::E_iso:
		*axis = 0;
		*parameter = surface->Domain(0).Max();
		return true;
	    case ON_Surface::S_iso:
		*axis = 1;
		*parameter = surface->Domain(1).Min();
		return true;
	    case ON_Surface::N_iso:
		*axis = 1;
		*parameter = surface->Domain(1).Max();
		return true;
	    default:
		break;
	}
	return false;
    };
    /* SetStartPoint()/SetEndPoint() on a supplied seam spline can perturb a
     * control point just far enough off its native W/E/S/N boundary for
     * ON_Surface::IsIsoparametric() to downgrade the curve.  Reprojecting only
     * the edited endpoint is not sufficient because the original exporter
     * may already have left sub-ulp drift in interior control points.
     *
     * Preserve the complete NURBS degree, knots, weights, domain, and varying
     * coordinate, but set the fixed homogeneous coordinate of every control
     * point to the already-proven native boundary.  Dense lift/edge validation
     * below remains authoritative; this helper only constructs an exactly
     * isoparametric candidate for that validation. */
    const auto seam_endpoint_candidate = [&boundary_parameter](
	    const ON_Surface *surface, const ON_BrepTrim &original,
	    const ON_3dPoint &endpoint, bool replace_start,
	    std::string *failure = NULL) -> ON_Curve * {
	if (failure)
	    failure->clear();
	if (!surface || original.m_type != ON_BrepTrim::seam) {
	    if (failure)
		*failure = "trim was not a seam on a valid surface";
	    return NULL;
	}
	int fixed_axis = -1;
	double fixed_parameter = ON_UNSET_VALUE;
	if (!boundary_parameter(surface, original.m_iso, &fixed_axis,
		&fixed_parameter)) {
	    if (failure)
		*failure = "seam did not have a native boundary ISO";
	    return NULL;
	}
	std::unique_ptr<ON_NurbsCurve> candidate(new ON_NurbsCurve());
	if (!original.GetNurbForm(*candidate)) {
	    if (failure)
		*failure = "seam NURBS conversion failed";
	    return NULL;
	}
	ON_3dPoint fixed_endpoint(endpoint);
	fixed_endpoint[fixed_axis] = fixed_parameter;
	fixed_endpoint.z = 0.0;
	if (!(replace_start ? candidate->SetStartPoint(fixed_endpoint) :
		candidate->SetEndPoint(fixed_endpoint))) {
	    if (failure)
		*failure = "seam NURBS endpoint replacement failed";
	    return NULL;
	}
	for (int cv_index = 0; cv_index < candidate->CVCount(); ++cv_index) {
	    ON_4dPoint cv;
	    if (!candidate->GetCV(cv_index, cv) || !cv.IsValid() ||
		    !(cv.w > 0.0)) {
		if (failure)
		    *failure = "seam NURBS had an invalid control point";
		return NULL;
	    }
	    if (fixed_axis == 0)
		cv.x = fixed_parameter * cv.w;
	    else
		cv.y = fixed_parameter * cv.w;
	    if (!candidate->SetCV(cv_index, cv)) {
		if (failure)
		    *failure = "seam NURBS boundary control-point projection failed";
		return NULL;
	    }
	}
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    if (failure)
		*failure = "projected seam NURBS was invalid";
	    return NULL;
	}
	const ON_Interval candidate_domain = candidate->Domain();
	const ON_Surface::ISO derived_iso =
	    surface->IsIsoparametric(*candidate, &candidate_domain);
	if (derived_iso != original.m_iso) {
	    if (failure)
		*failure = "projected seam ISO was " +
		    std::to_string(static_cast<int>(derived_iso)) +
		    " instead of " +
		    std::to_string(static_cast<int>(original.m_iso));
	    return NULL;
	}
	return candidate.release();
    };
    std::set<int> attempted_edge_regeneration;
    std::set<int> measured_join_trims;
    std::set<int> measured_source_join_trims;
    std::set<int> translated_trim_indices;
    std::map<int, std::set<std::vector<double> > > seen_loop_states;
    ON_BoundingBox item_bounds;
    const double item_scale = brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;

    const auto loop_endpoint_state = [](const ON_BrepLoop &loop) {
	std::vector<double> state;
	state.reserve(static_cast<size_t>(loop.TrimCount()) * 8);
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim) {
		state.push_back(ON_UNSET_VALUE);
		continue;
	    }
	    const ON_3dPoint start = trim->PointAtStart();
	    const ON_3dPoint end = trim->PointAtEnd();
	    state.push_back(start.x);
	    state.push_back(start.y);
	    state.push_back(end.x);
	    state.push_back(end.y);
	    state.push_back(static_cast<double>(trim->m_type));
	    state.push_back(static_cast<double>(trim->m_iso));
	    state.push_back(static_cast<double>(trim->m_vi[0]));
	    state.push_back(static_cast<double>(trim->m_vi[1]));
	}
	return state;
    };

    std::vector<bool> dirty(static_cast<size_t>(brep->m_L.Count()), false);
    if (only_loop >= 0) {
	dirty[static_cast<size_t>(only_loop)] = true;
	} else {
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    const ON_BrepFace *face = brep->m_L[li].Face();
	    dirty[static_cast<size_t>(li)] = !face_source_tags ||
		(face && face_source_tags->find(face->m_face_user.i) !=
		    face_source_tags->end());
	}
    }

    for (int sweep = 0; sweep < kMaximumEndpointRepairSweeps; ++sweep) {
	if (brlcad::PullbackWorkCancelled())
	    return;
	if (record_changes)
	    wrapper->SetProgressDetail(
		"repairing adjacent exact BREP trim endpoints", entity_id, sweep,
		kMaximumEndpointRepairSweeps, "sweeps", entity_type);
	bool changed = false;
	std::vector<bool> next_dirty(static_cast<size_t>(brep->m_L.Count()), false);
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    if (!dirty[static_cast<size_t>(li)])
		continue;
	    if (brlcad::PullbackWorkCancelled())
		return;
	    ON_BrepLoop &loop = brep->m_L[li];
	    /* A sequence of endpoint-local repairs can otherwise oscillate between
	     * two equivalent periodic branches.  Exact endpoint state is a complete
	     * key for this propagation pass; revisiting one proves that another
	     * sweep cannot add information. */
	    const std::vector<double> starting_state = loop_endpoint_state(loop);
	    if (!seen_loop_states[li].insert(starting_state).second)
		continue;
	    bool loop_changed = false;
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    if (loop.TrimCount() < 2)
		continue;
	    const int join_count = skip_closing_join ?
		loop.TrimCount() - 1 : loop.TrimCount();
	    for (int lti = 0; lti < join_count; ++lti) {
		if (brlcad::PullbackWorkCancelled())
		    return;
		ON_BrepTrim *previous = loop.Trim(lti);
		ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!previous || !next || previous == next ||
			previous->m_vi[1] < 0 ||
			previous->m_vi[1] != next->m_vi[0] ||
			previous->m_vi[1] >= brep->m_V.Count())
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		if (previous_uv.DistanceTo(next_uv) <= ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
		double join_tolerance = LocalUnits::tolerance;
		if (previous->Edge())
		    join_tolerance = std::max(join_tolerance,
			previous->Edge()->m_tolerance);
		if (next->Edge())
		    join_tolerance = std::max(join_tolerance,
			next->Edge()->m_tolerance);
		join_tolerance = std::max(join_tolerance,
		    std::max(previous->m_tolerance[0], previous->m_tolerance[1]));
		join_tolerance = std::max(join_tolerance,
		    std::max(next->m_tolerance[0], next->m_tolerance[1]));
		join_tolerance = std::max(join_tolerance,
		    brep->m_V[previous->m_vi[1]].m_tolerance);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		/* If an exporter asserted that an exact 3-D edge lies on this
		 * surface but the closest surface locus genuinely misses the shared
		 * topology vertex, the declared uncertainty is not an achievable
		 * OpenNURBS tolerance for this one association.  Safe mode may use a
		 * larger local tolerance only after measuring the complete immutable
		 * edge against the surface and applying the ordinary scale bounds.
		 * This proof is needed for ordinary joins as well as pole joins:
		 * rejecting the endpoint before measuring it leaves a repairable loop
		 * open.  --exact and --repair none never enter this path. */
		if (!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe &&
			previous_lift.IsValid() && next_lift.IsValid() &&
			(previous_lift.DistanceTo(vertex) > join_tolerance ||
			 next_lift.DistanceTo(vertex) > join_tolerance)) {
		    ON_BrepTrim *join_trims[2] = {previous, next};
		    for (int join_side = 0; join_side < 2; ++join_side) {
			ON_BrepTrim *join_trim = join_trims[join_side];
			ON_BrepEdge *join_edge = join_trim ?
			    join_trim->Edge() : NULL;
			ON_NurbsCurve join_edge_nurbs;
			if (!join_trim || !join_edge ||
				!measured_join_trims.insert(
				    join_trim->m_trim_index).second ||
				!join_edge->GetNurbForm(join_edge_nurbs))
			    continue;
			join_tolerance = std::max(join_tolerance,
			    verified_regeneration_tolerance(*join_trim,
				*join_edge, surface, join_edge_nurbs,
				join_tolerance, brep, wrapper, entity_id,
				entity_type));
		    }
		}
		/* The exact 3-D edge may lie on the surface even though the
		 * exporter's supplied pcurve follows that association with a small,
		 * scale-bounded offset.  In that case the surface-regeneration
		 * measurement above correctly returns no larger tolerance, but
		 * rejecting the join before measuring the existing pcurve prevents
		 * the later transactional endpoint repair from running.  Measure the
		 * complete pcurve/edge association once per trim and use its bounded
		 * safe-mode tolerance only to admit the candidate construction below.
		 * The candidate still has to pass dense edge, lift, and shared-vertex
		 * validation; --exact bypasses this path. */
		if (!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe &&
			previous_lift.IsValid() && next_lift.IsValid() &&
			(previous_lift.DistanceTo(vertex) > join_tolerance ||
			 next_lift.DistanceTo(vertex) > join_tolerance)) {
		    ON_BrepTrim *join_trims[2] = {previous, next};
		    for (int join_side = 0; join_side < 2; ++join_side) {
			ON_BrepTrim *join_trim = join_trims[join_side];
			ON_BrepEdge *join_edge = join_trim ?
			    join_trim->Edge() : NULL;
			ON_NurbsCurve join_edge_nurbs;
			if (!join_trim || !join_edge ||
				!measured_source_join_trims.insert(
				    join_trim->m_trim_index).second ||
				!join_edge->GetNurbForm(join_edge_nurbs))
			    continue;
			const double existing_tolerance = std::max(
			    LocalUnits::tolerance,
			    std::max(join_edge->m_tolerance,
				std::max(join_trim->m_tolerance[0],
				    join_trim->m_tolerance[1])));
			join_tolerance = std::max(join_tolerance,
			    verified_source_pcurve_tolerance(*join_trim,
				*join_edge, surface, join_edge_nurbs,
				existing_tolerance, brep, wrapper, entity_id,
				entity_type));
		    }
		}
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > join_tolerance ||
			next_lift.DistanceTo(vertex) > join_tolerance) {
		    /* The shared topology vertex is the authoritative join.  Two
		     * independently generated pcurve endpoints can lie on opposite
		     * sides of that vertex and therefore be as much as twice the model
		     * uncertainty apart even though each endpoint is individually
		     * valid.  Candidate construction and dense validation below still
		     * require the installed common endpoint to stay within the model
		     * uncertainty of both the exact edge and its original surface lift. */
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " endpoint precheck rejected trims "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " vertex distances=" << previous_lift.DistanceTo(vertex)
			    << '/' << next_lift.DistanceTo(vertex) << " lift="
			    << previous_lift.DistanceTo(next_lift) << std::endl;
		    continue;
		}

		/* A full-period UV gap at a collapsed surface boundary is not an
		 * ordinary branch mismatch.  It is the parameter-space span of a
		 * required singular connector.  Snapping either neighboring pcurve
		 * across that gap can collapse a short, distinct-vertex STEP edge
		 * into a closed c2 curve, which OpenNURBS correctly rejects.
		 *
		 * Preserve the gap only after both endpoint lifts have passed the
		 * shared topology-vertex proof above and the surface differential
		 * independently proves a singular point at both periodic images.
		 * repair_missing_singular_trims() materializes the connector; ordinary
		 * closed-surface seams, whose differential has full rank, continue
		 * through the normal branch solver. */
		const auto singular_parameter = [surface](const ON_3dPoint &uv) {
		    const ON_3dPoint native =
			closed_surface_native_parameter(surface, uv);
		    if (!native.IsValid())
			return false;
		    if (surface->IsAtSingularity(native.x, native.y, false))
			return true;
		    ON_3dPoint point;
		    ON_3dVector du, dv;
		    if (!surface->Ev1Der(native.x, native.y, point, du, dv))
			return false;
		    const double a = du * du;
		    const double b = du * dv;
		    const double c = dv * dv;
		    const double determinant = a * c - b * b;
		    const double numerical_floor = ON_ZERO_TOLERANCE *
			std::max(1.0, a * c);
		    return fabs(determinant) <= numerical_floor;
		};
		bool required_singular_connector = false;
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    const double delta =
			previous_uv[direction] - next_uv[direction];
		    const double turns = round(delta / period);
		    const double parameter_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			period * kPeriodicParameterSnapFraction);
		    if (fabs(turns) < 0.5 ||
			    fabs(delta - turns * period) >
				parameter_tolerance)
			continue;
		    required_singular_connector =
			singular_parameter(previous_uv) &&
			singular_parameter(next_uv);
		    if (required_singular_connector)
			break;
		}
		if (required_singular_connector &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << "/STEP" << loop.m_loop_user.i
			    << " retained full-period pole gap between trims "
			    << previous->m_trim_index << '/'
			    << next->m_trim_index
			    << " for an exact singular connector" << std::endl;
		    continue;
		}

		/* A trim approaching a collapsed surface boundary can expose a
		 * source edge/surface separation only in the final few dense
		 * samples.  Before editing either endpoint, measure the immutable
		 * ordinary STEP edge against the complete surface.  Normal safe mode
		 * may then reflect a small, scale-bounded source discrepancy in this
		 * one edge/trim tolerance; --exact leaves the declared uncertainty
		 * unchanged.  This proof is deliberately performed once per affected
		 * trim and does not validate an endpoint edit merely because it is
		 * nearby. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular)) {
		    ON_BrepTrim *ordinary = previous->m_type ==
			ON_BrepTrim::singular ? next : previous;
		    ON_BrepEdge *ordinary_edge = ordinary ? ordinary->Edge() : NULL;
		    ON_NurbsCurve ordinary_nurbs;
		    if (ordinary && ordinary_edge &&
			    measured_join_trims.insert(
				ordinary->m_trim_index).second &&
			    ordinary_edge->GetNurbForm(ordinary_nurbs)) {
			join_tolerance = std::max(join_tolerance,
			    verified_regeneration_tolerance(*ordinary,
				*ordinary_edge, surface, ordinary_nurbs,
				join_tolerance, brep, wrapper, entity_id,
				entity_type));
		    }
		}

		/* Native-seam splitting can turn one STEP edge use into two
		 * consecutive OpenNURBS seam fragments.  When the split falls within
		 * source tolerance of the periodic cut, one fragment can end just
		 * above the domain minimum while the next begins at the equivalent
		 * domain maximum.  Moving only that endpoint would bend a short seam
		 * fragment through an entire surface period.  First translate the
		 * complete following fragment by the exact varying-coordinate period;
		 * validate_periodic_trim_translation() proves identical surface lift,
		 * and the ordinary endpoint candidate below then handles only the
		 * residual source discrepancy. */
		ON_BrepEdge *previous_edge = previous->Edge();
		ON_BrepEdge *next_edge = next->Edge();
		if (previous->m_type == ON_BrepTrim::seam &&
			next->m_type == ON_BrepTrim::seam &&
			previous->m_iso == next->m_iso &&
			previous_edge && next_edge &&
			previous_edge->m_edge_user.i > 0 &&
			previous_edge->m_edge_user.i ==
			    next_edge->m_edge_user.i) {
		    std::string translation_failure;
		    double bounded_lift_tolerance = -1.0;
		    if (!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe) {
			bounded_lift_tolerance = join_tolerance *
			    kPeriodicClosureToleranceMaximumScale;
			/* Some exporters mark a fitted NURBS surface closed even
			 * though raw OpenNURBS evaluation at its two domain sides
			 * differs by more than twice the declared uncertainty.  A
			 * whole-period pcurve translation is still usable when both
			 * its surface lift and immutable 3-D edge association pass
			 * the same local-feature/item-scale bounds used by exact
			 * pullback regeneration.  The dense validator below measures
			 * every accepted translation; this is only its rejection
			 * ceiling, not the installed tolerance. */
			if (next_edge) {
			    const ON_BoundingBox edge_bounds =
				next_edge->BoundingBox();
			    const double edge_scale = edge_bounds.IsValid() ?
				edge_bounds.Diagonal().Length() : 0.0;
			    bounded_lift_tolerance = std::max(
				bounded_lift_tolerance,
				std::max(edge_scale *
				    kRegenerationMaximumRelativeMismatch,
				    item_scale *
				    kRegenerationMaximumRelativeItemMismatch));
			}
		    }
		    double measured_bounded_mismatch = 0.0;
		    ON_Curve *translated = translated_trim_indices.find(
			next->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *next,
			    previous_uv, true, &translation_failure,
			    bounded_lift_tolerance,
			    &measured_bounded_mismatch) : NULL;
		    double adjusted_tolerance = join_tolerance;
		    if (translated && measured_bounded_mismatch > join_tolerance) {
			adjusted_tolerance = measured_bounded_mismatch *
			    kRegenerationToleranceSafety;
			if (adjusted_tolerance > bounded_lift_tolerance) {
			    std::ostringstream reason;
			    reason << "measured closed-surface drift "
				<< std::setprecision(17)
				<< measured_bounded_mismatch
				<< " required tolerance " << adjusted_tolerance
				<< " beyond bounded limit "
				<< bounded_lift_tolerance;
			    translation_failure = reason.str();
			    delete translated;
			    translated = NULL;
			}
		    }
		    const ON_Interval translated_domain = translated ?
			translated->Domain() : ON_Interval::EmptyInterval;
		    if (translated &&
			    surface->IsIsoparametric(*translated,
				&translated_domain) != next->m_iso) {
			translation_failure =
			    "translated split seam fragment changed native ISO";
			delete translated;
			translated = NULL;
		    }
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*next, c2_index)) {
			    brep->SetTrimIsoFlags(*next);
			    if (adjusted_tolerance > join_tolerance) {
				next_edge->m_tolerance = std::max(
				    next_edge->m_tolerance, adjusted_tolerance);
				next->m_tolerance[0] = std::max(
				    next->m_tolerance[0], adjusted_tolerance);
				next->m_tolerance[1] = std::max(
				    next->m_tolerance[1], adjusted_tolerance);
				if (record_changes) {
				    wrapper->RecordDiagnostic(
					brlcad::step::DiagnosticSeverity::Warning,
					entity_id, entity_type, "trim_pcurve",
					"closed-surface period drift exceeded the declared "
					"tolerance; used a densely measured split-seam "
					"tolerance");
				    wrapper->RecordRepair(entity_id, entity_type,
					"trim_pcurve",
					"adjusted one split-seam tolerance to measured "
					"closed-surface drift");
				}
			    }
			    record_repair("trim_pcurve",
				"translated an exact split seam fragment onto its adjacent periodic branch");
			    translated_trim_indices.insert(next->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic split-seam join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;
		}

		/* Adjacent ordinary trims can be returned on different periodic
		 * images of the same closed surface, including doubly periodic tori.
		 * Move the complete following pcurve by integral surface periods
		 * before considering any endpoint-local edit.  Dense lift validation
		 * proves this is a parameter-branch change only. */
		if (previous->m_type != ON_BrepTrim::seam &&
			next->m_type != ON_BrepTrim::seam &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    std::string translation_failure;
		    double bounded_lift_tolerance = -1.0;
		    if (!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe) {
			bounded_lift_tolerance = join_tolerance *
			    kPeriodicClosureToleranceMaximumScale;
			if (next_edge) {
			    const ON_BoundingBox edge_bounds =
				next_edge->BoundingBox();
			    const double edge_scale = edge_bounds.IsValid() ?
				edge_bounds.Diagonal().Length() : 0.0;
			    bounded_lift_tolerance = std::max(
				bounded_lift_tolerance,
				std::max(edge_scale *
				    kRegenerationMaximumRelativeMismatch,
				    item_scale *
				    kRegenerationMaximumRelativeItemMismatch));
			}
		    }
		    double measured_bounded_mismatch = 0.0;
		    ON_Curve *translated = translated_trim_indices.find(
			next->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *next,
			    previous_uv, true, &translation_failure,
			    bounded_lift_tolerance,
			    &measured_bounded_mismatch) : NULL;
		    double adjusted_tolerance = join_tolerance;
		    if (translated && measured_bounded_mismatch >
			    join_tolerance) {
			adjusted_tolerance = measured_bounded_mismatch *
			    kRegenerationToleranceSafety;
			if (adjusted_tolerance > bounded_lift_tolerance) {
			    std::ostringstream reason;
			    reason << "measured closed-surface drift "
				<< std::setprecision(17)
				<< measured_bounded_mismatch
				<< " required tolerance " << adjusted_tolerance
				<< " beyond bounded limit "
				<< bounded_lift_tolerance;
			    translation_failure = reason.str();
			    delete translated;
			    translated = NULL;
			}
		    }
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*next, c2_index)) {
			    brep->SetTrimIsoFlags(*next);
			    if (adjusted_tolerance > join_tolerance) {
				if (next_edge)
				    next_edge->m_tolerance = std::max(
					next_edge->m_tolerance,
					adjusted_tolerance);
				next->m_tolerance[0] = std::max(
				    next->m_tolerance[0], adjusted_tolerance);
				next->m_tolerance[1] = std::max(
				    next->m_tolerance[1], adjusted_tolerance);
				join_tolerance = adjusted_tolerance;
				if (record_changes) {
				    wrapper->RecordDiagnostic(
					brlcad::step::DiagnosticSeverity::Warning,
					entity_id, entity_type, "trim_pcurve",
					"closed-surface period drift exceeded the declared "
					"tolerance; used a densely measured local "
					"OpenNURBS tolerance");
				    wrapper->RecordRepair(entity_id, entity_type,
					"trim_pcurve",
					"adjusted one periodic pcurve tolerance to "
					"measured closed-surface drift");
				}
			    }
			    record_repair("trim_pcurve",
				"translated an exact pcurve onto its adjacent periodic branch");
			    translated_trim_indices.insert(next->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    /* A surface can report a closed parameter direction without
		     * providing literal periodic evaluation outside its native
		     * domain.  In that case a whole-period translation correctly
		     * aligns this endpoint but changes the interior lift, so the
		     * dense translation proof above rejects it.  Re-pull the exact
		     * bounded edge with both endpoint images constrained instead:
		     * this permits the pcurve to follow the surface's native branch
		     * between two lift-equivalent images without moving either the
		     * STEP edge or the surface. */
		    bool integral_period_gap = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction))
			    continue;
			const double period =
			    surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			const double delta =
			    previous_uv[direction] - next_uv[direction];
			const double turns = round(delta / period);
			const double parameter_tolerance = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    period * kPeriodicParameterSnapFraction);
			if (fabs(turns) >= 0.5 &&
				fabs(delta - turns * period) <=
				    parameter_tolerance) {
			    integral_period_gap = true;
			    break;
			}
		    }
		    std::string constrained_failure;
		    ON_Curve *constrained_curve = NULL;
		    ON_NurbsCurve next_edge_nurbs;
		    const ON_3dPoint required_end = next->PointAtEnd();
		    const bool constrained = !translated &&
			integral_period_gap && next_edge &&
			next_edge->GetNurbForm(next_edge_nurbs) &&
			regenerate_trim_polyline(brep, *next, surface,
			    next_edge_nurbs, join_tolerance,
			    &constrained_failure, NULL, &previous_uv,
			    &required_end, true, wrapper, true,
			    &constrained_curve);
		    if (constrained && constrained_curve) {
			const int c2_index =
			    brep->AddTrimCurve(constrained_curve);
			if (c2_index >= 0 &&
				brep->SetTrimCurve(*next, c2_index)) {
			    brep->SetTrimIsoFlags(*next);
			    record_repair("trim_pcurve",
				"regenerated an exact pcurve between lift-equivalent "
				"periodic endpoint images");
			    translated_trim_indices.insert(next->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete constrained_curve;
		    } else {
			delete constrained_curve;
		    }
		    if (!translated && wrapper->Verbose() &&
			    (!translation_failure.empty() ||
			     !constrained_failure.empty()))
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic ordinary join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: "
			    << (translation_failure.empty() ?
				"no integral translation candidate" :
				translation_failure)
			    << (constrained_failure.empty() ? "" :
				"; constrained exact-edge pullback: " +
				    constrained_failure)
			    << std::endl;
		}

		/* A singular trim is a genuine zero-length boundary at a surface
		 * pole.  When its ordinary neighbor arrives on the opposite periodic
		 * branch, move that complete neighbor by an exact period before any
		 * endpoint-local edit. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular)) {
		    ON_BrepTrim *movable = previous->m_type == ON_BrepTrim::singular ?
			next : previous;
		    const ON_3dPoint singular_join = previous->m_type ==
			ON_BrepTrim::singular ? previous_uv : next_uv;
		    const bool move_start = movable == next;
		    std::string translation_failure;
		    ON_Curve *translated = translated_trim_indices.find(
			movable->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *movable,
			    singular_join, move_start, &translation_failure) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
			    record_repair("trim_pcurve",
				"shifted an exact pcurve onto a singular trim's periodic branch");
			    translated_trim_indices.insert(movable->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic singular join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;
		}

		/* When exactly one side is a seam, first try moving the entire
		 * non-seam pcurve by integral closed-surface periods.  This preserves
		 * its shape and edge correspondence, unlike bending only its endpoint. */
		if ((previous->m_type == ON_BrepTrim::seam) !=
			(next->m_type == ON_BrepTrim::seam) &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    ON_BrepTrim *movable = previous->m_type == ON_BrepTrim::seam ?
			next : previous;
		    ON_BrepEdge *movable_edge = movable->Edge();
		    const ON_3dPoint seam_join = movable == previous ? next_uv :
			previous_uv;
		    std::string translation_failure;
		    double bounded_lift_tolerance = -1.0;
		    if (!wrapper->ImportOptions().exact &&
			    wrapper->ImportOptions().repair ==
				brlcad::step::RepairMode::Safe) {
			bounded_lift_tolerance = join_tolerance *
			    kPeriodicClosureToleranceMaximumScale;
			if (movable_edge) {
			    const ON_BoundingBox edge_bounds =
				movable_edge->BoundingBox();
			    const double edge_scale = edge_bounds.IsValid() ?
				edge_bounds.Diagonal().Length() : 0.0;
			    bounded_lift_tolerance = std::max(
				bounded_lift_tolerance,
				std::max(edge_scale *
				    kRegenerationMaximumRelativeMismatch,
				    item_scale *
				    kRegenerationMaximumRelativeItemMismatch));
			}
		    }
		    double measured_bounded_mismatch = 0.0;
		    ON_Curve *translated = translated_trim_indices.find(
			movable->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *movable,
			    seam_join, movable == next, &translation_failure,
			    bounded_lift_tolerance,
			    &measured_bounded_mismatch) : NULL;
		    double adjusted_tolerance = join_tolerance;
		    if (translated && measured_bounded_mismatch >
			    join_tolerance) {
			adjusted_tolerance = measured_bounded_mismatch *
			    kRegenerationToleranceSafety;
			if (adjusted_tolerance > bounded_lift_tolerance) {
			    std::ostringstream reason;
			    reason << "measured closed-surface drift "
				<< std::setprecision(17)
				<< measured_bounded_mismatch
				<< " required tolerance " << adjusted_tolerance
				<< " beyond bounded limit "
				<< bounded_lift_tolerance;
			    translation_failure = reason.str();
			    delete translated;
			    translated = NULL;
			}
		    }
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
			    if (adjusted_tolerance > join_tolerance) {
				if (movable_edge)
				    movable_edge->m_tolerance = std::max(
					movable_edge->m_tolerance,
					adjusted_tolerance);
				movable->m_tolerance[0] = std::max(
				    movable->m_tolerance[0], adjusted_tolerance);
				movable->m_tolerance[1] = std::max(
				    movable->m_tolerance[1], adjusted_tolerance);
				join_tolerance = adjusted_tolerance;
				if (record_changes) {
				    wrapper->RecordDiagnostic(
					brlcad::step::DiagnosticSeverity::Warning,
					entity_id, entity_type, "trim_pcurve",
					"closed-surface period drift exceeded the "
					"declared tolerance at a seam boundary; used a "
					"densely measured local OpenNURBS tolerance");
				    wrapper->RecordRepair(entity_id, entity_type,
					"trim_pcurve",
					"adjusted one seam-boundary pcurve tolerance "
					"to measured closed-surface drift");
				}
			    }
			    record_repair("trim_pcurve",
				"shifted an exact non-seam pcurve onto an adjacent periodic branch");
			    translated_trim_indices.insert(movable->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic seam join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;

		    /* A non-seam edge can meet a periodic seam at a parameter that is
		     * lift-equivalent, but not an integral-period translation of its
		     * supplied endpoint.  Seed an exact edge pullback at the seam's UV
		     * endpoint so the regenerated curve leaves the correct side of the
		     * domain and remains continuous with the loop. */
		    ON_NurbsCurve movable_edge_nurbs;
		    std::string seeded_failure;
		    ON_3dPoint other_seam_join = ON_3dPoint::UnsetPoint;
		    const ON_3dPoint *required_start = movable == next ?
			&seam_join : NULL;
		    const ON_3dPoint *required_end = movable == previous ?
			&seam_join : NULL;
		    if (movable == next) {
			ON_BrepTrim *following = loop.Trim(
			    (lti + 2) % loop.TrimCount());
			if (following && following->m_type == ON_BrepTrim::seam &&
				movable->m_vi[1] >= 0 &&
				movable->m_vi[1] == following->m_vi[0] &&
				movable->m_vi[1] < brep->m_V.Count()) {
			    other_seam_join = following->PointAtStart();
			    const ON_3dPoint other_lift =
				closed_surface_point_at(surface, other_seam_join);
			    if (other_lift.IsValid() && other_lift.DistanceTo(
				    brep->m_V[movable->m_vi[1]].point) <=
					LocalUnits::tolerance)
				required_end = &other_seam_join;
			}
		    } else {
			ON_BrepTrim *preceding = loop.Trim((lti +
			    loop.TrimCount() - 1) % loop.TrimCount());
			if (preceding && preceding->m_type == ON_BrepTrim::seam &&
				movable->m_vi[0] >= 0 &&
				preceding->m_vi[1] == movable->m_vi[0] &&
				movable->m_vi[0] < brep->m_V.Count()) {
			    other_seam_join = preceding->PointAtEnd();
			    const ON_3dPoint other_lift =
				closed_surface_point_at(surface, other_seam_join);
			    if (other_lift.IsValid() && other_lift.DistanceTo(
				    brep->m_V[movable->m_vi[0]].point) <=
					LocalUnits::tolerance)
				required_start = &other_seam_join;
			}
		    }
		    const bool seeded = movable_edge &&
			movable_edge->GetNurbForm(movable_edge_nurbs) &&
		    regenerate_trim_polyline(brep, *movable, surface,
			    movable_edge_nurbs, LocalUnits::tolerance,
			    &seeded_failure, NULL, required_start, required_end,
			    false, wrapper,
			    surface->IsClosed(0) && surface->IsClosed(1));
		    const ON_3dPoint seeded_join = seeded ?
			(movable == previous ? movable->PointAtEnd() :
			 movable->PointAtStart()) : ON_3dPoint::UnsetPoint;
		    if (seeded && seeded_join.IsValid() &&
			    seeded_join.DistanceTo(seam_join) <= ON_ZERO_TOLERANCE) {
			record_repair("trim_pcurve",
			    "regenerated a non-seam pcurve from an exact periodic seam endpoint");
			changed = true;
			loop_changed = true;
			continue;
		    }
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " seam-seeded regeneration for trim "
			    << movable->m_trim_index << " rejected: regenerated="
			    << (seeded ? "yes" : "no") << " join="
			    << (seeded_join.IsValid() ?
				seeded_join.DistanceTo(seam_join) : DBL_MAX)
			    << (seeded_failure.empty() ? "" :
				", detail=" + seeded_failure) << std::endl;
		}

		ON_3dPoint aligned_next = next_uv;
		for (int axis = 0; axis < 2; ++axis) {
		    if (!surface->IsClosed(axis))
			continue;
		    const double period = surface->Domain(axis).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint candidate = aligned_next;
		    candidate[axis] += round((previous_uv[axis] - candidate[axis]) /
			period) * period;
		    const ON_3dPoint candidate_lift =
			closed_surface_point_at(surface, candidate);
		    if (candidate_lift.IsValid() && candidate_lift.DistanceTo(next_lift) <=
			    std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
				LocalUnits::tolerance * 1.0e-8))
			aligned_next = candidate;
		}
		ON_3dPoint common = 0.5 * (previous_uv + aligned_next);
		for (int axis = 0; axis < 2; ++axis) {
		    const bool previous_boundary = boundary_axis(previous->m_iso, axis);
		    const bool next_boundary = boundary_axis(next->m_iso, axis);
		    if (previous_boundary != next_boundary)
			common[axis] = previous_boundary ? previous_uv[axis] : aligned_next[axis];
		}
		if ((previous->m_type == ON_BrepTrim::seam) !=
			(next->m_type == ON_BrepTrim::seam))
		    common = previous->m_type == ON_BrepTrim::seam ?
			previous_uv : next_uv;
		/* Every UV along a collapsed side has the same 3-D lift.  Preserve
		 * the singular trim's exact fixed boundary coordinate, but take its
		 * undefined periodic coordinate from the ordinary edge's limiting
		 * endpoint.  Treating the whole singular endpoint as authoritative
		 * can bend an otherwise exact spherical meridian through an arbitrary
		 * pole longitude, producing a zero-area hook which shaded-mesh repair
		 * may detach. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular)) {
		    const ON_BrepTrim *singular =
			previous->m_type == ON_BrepTrim::singular ? previous : next;
		    const ON_3dPoint singular_uv =
			previous->m_type == ON_BrepTrim::singular ? previous_uv : next_uv;
		    const ON_3dPoint ordinary_uv =
			previous->m_type == ON_BrepTrim::singular ? next_uv : previous_uv;
		    common = singular_uv;
		    int fixed_axis = -1;
		    double fixed_parameter = ON_UNSET_VALUE;
		    if (boundary_parameter(surface, singular->m_iso, &fixed_axis,
			    &fixed_parameter)) {
			common[fixed_axis] = fixed_parameter;
			common[1 - fixed_axis] = ordinary_uv[1 - fixed_axis];
		    }
		}

		/* The shared STEP vertex is the authoritative model-space join.  On a
		 * closed surface, averaging two independently fitted UV endpoints can
		 * select a third lift between them.  That midpoint may be within the
		 * measured join tolerance, but subsequent exact-period translations
		 * can then propagate its artificial phase around the loop.  Prefer the
		 * vertex's own surface image when it is a strictly better model-space
		 * match and remains within the already proven bound of both supplied
		 * endpoint lifts.  Dense whole-curve validation below still has to
		 * authorize every resulting endpoint-local edit. */
		if (previous->m_type != ON_BrepTrim::seam &&
			next->m_type != ON_BrepTrim::seam &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    brlcad::PullbackContext vertex_context;
		    ON_2dPoint vertex_uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint vertex_lift;
		    double vertex_distance = DBL_MAX;
		    const double close_tolerance = std::max(ON_ZERO_TOLERANCE,
			std::min(join_tolerance, LocalUnits::tolerance));
		    bool projected = vertex_context.SurfaceClosestPoint(surface,
			vertex, vertex_uv, vertex_lift, vertex_distance, 0,
			std::max(ON_ZERO_TOLERANCE, close_tolerance * 0.1),
			close_tolerance);
		    if (!projected && join_tolerance > close_tolerance)
			projected = vertex_context.SurfaceClosestPoint(surface,
			    vertex, vertex_uv, vertex_lift, vertex_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				join_tolerance * 0.1), join_tolerance);
		    if (projected && vertex_distance <= join_tolerance) {
			ON_3dPoint projected_common(vertex_uv.x, vertex_uv.y, 0.0);
			for (int axis = 0; axis < 2; ++axis) {
			    if (!surface->IsClosed(axis))
				continue;
			    const double period = surface->Domain(axis).Length();
			    if (period > ON_ZERO_TOLERANCE)
				projected_common[axis] += round((common[axis] -
				    projected_common[axis]) / period) * period;
			}
			const ON_3dPoint projected_lift =
			    closed_surface_point_at(surface, projected_common);
			const ON_3dPoint initial_lift =
			    closed_surface_point_at(surface, common);
			const double numerical_tolerance = ON_ZERO_TOLERANCE *
			    kNumericalToleranceScale;
			if (projected_lift.IsValid() &&
				projected_lift.DistanceTo(vertex) <= join_tolerance &&
				projected_lift.DistanceTo(previous_lift) <=
				    join_tolerance &&
				projected_lift.DistanceTo(next_lift) <=
				    join_tolerance &&
				(!initial_lift.IsValid() ||
				 projected_lift.DistanceTo(vertex) +
				     numerical_tolerance <
				 initial_lift.DistanceTo(vertex)))
			    common = projected_common;
		    }
		}

		/* Parameter-space midpoint is not a model-space midpoint on a
		 * nonlinear surface.  When two independently supplied endpoints are
		 * each valid at their shared STEP vertex but their lifts differ by up
		 * to twice the file uncertainty, search the bounded UV chord for a
		 * common lift that remains within the declared tolerance of both.
		 * This changes only trim association; the exact 3-D edges are retained
		 * and the complete candidate curves are densely checked below. */
		if (previous->m_type != ON_BrepTrim::seam &&
			next->m_type != ON_BrepTrim::seam &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    const ON_3dPoint initial_lift =
			closed_surface_point_at(surface, common);
		    const bool initial_common_valid = initial_lift.IsValid() &&
			initial_lift.DistanceTo(vertex) <= join_tolerance &&
			initial_lift.DistanceTo(previous_lift) <= join_tolerance &&
			initial_lift.DistanceTo(next_lift) <= join_tolerance;
		    if (!initial_common_valid) {
			double best_score = DBL_MAX;
			ON_3dPoint best_common = ON_3dPoint::UnsetPoint;
			for (int sample = 0; sample <= kDenseValidationSegments;
				++sample) {
			    if ((sample & 63) == 0 &&
				    brlcad::PullbackWorkCancelled())
				return;
			    const double fraction = static_cast<double>(sample) /
				kDenseValidationSegments;
			    const ON_3dPoint candidate = (1.0 - fraction) * previous_uv +
				fraction * aligned_next;
			    const ON_3dPoint lift =
				closed_surface_point_at(surface, candidate);
			    if (!lift.IsValid())
				continue;
			    const double vertex_distance = lift.DistanceTo(vertex);
			    const double previous_distance = lift.DistanceTo(previous_lift);
			    const double next_distance = lift.DistanceTo(next_lift);
			    if (vertex_distance > join_tolerance ||
				    previous_distance > join_tolerance ||
				    next_distance > join_tolerance)
				continue;
			    const double score = std::max(vertex_distance,
				std::max(previous_distance, next_distance));
			    if (score < best_score) {
				best_score = score;
				best_common = candidate;
			    }
			}
			if (best_common.IsValid())
			    common = best_common;
		    }
		}
		const ON_3dPoint common_lift =
		    closed_surface_point_at(surface, common);
		if (!common_lift.IsValid() ||
			common_lift.DistanceTo(vertex) > join_tolerance) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " common endpoint rejected for trims "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " at " << common.x << ':' << common.y << " vertex distance="
			    << common_lift.DistanceTo(vertex) << std::endl;
		    continue;
		}

		/* A seam fragment ending at a collapsed boundary must approach the
		 * pole on the limiting parameter branch selected by its adjacent
		 * singular trim.  Editing only the seam endpoint can bend a supplied
		 * spline through an invalid edge locus.  Reconstruct the complete
		 * seam fragment from its immutable 3-D STEP edge with both loop
		 * endpoints fixed, and install it only after the ordinary dense
		 * pullback validation succeeds. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular) &&
			((previous->m_type == ON_BrepTrim::seam) !=
			 (next->m_type == ON_BrepTrim::seam))) {
		    ON_BrepTrim *seam = previous->m_type == ON_BrepTrim::seam ?
			previous : next;
		    ON_BrepEdge *seam_edge = seam ? seam->Edge() : NULL;
		    ON_NurbsCurve seam_nurbs;
		    const ON_3dPoint seam_other = seam == previous ?
			seam->PointAtStart() : seam->PointAtEnd();
		    const ON_3dPoint *required_start = seam == previous ?
			&seam_other : &common;
		    const ON_3dPoint *required_end = seam == previous ?
			&common : &seam_other;
		    ON_Curve *regenerated_curve = NULL;
		    std::string regeneration_failure;
		    const bool regenerated = seam_edge &&
			seam_edge->GetNurbForm(seam_nurbs) &&
			regenerate_trim_polyline(brep, *seam, surface,
			    seam_nurbs, join_tolerance, &regeneration_failure,
			    NULL, required_start, required_end, true, wrapper,
			    true, &regenerated_curve);
		    const ON_3dPoint regenerated_join = regenerated_curve ?
			(seam == previous ? regenerated_curve->PointAtEnd() :
			 regenerated_curve->PointAtStart()) :
			ON_3dPoint::UnsetPoint;
		    if (regenerated && regenerated_curve &&
			    regenerated_join.DistanceTo(common) <=
				ON_ZERO_TOLERANCE) {
			const int c2_index = brep->AddTrimCurve(regenerated_curve);
			if (c2_index >= 0 && brep->SetTrimCurve(*seam,
				c2_index)) {
			    brep->SetTrimIsoFlags(*seam);
			    record_repair("trim_pcurve",
				"regenerated a seam fragment from an exact surface-pole endpoint");
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete regenerated_curve;
		    } else {
			delete regenerated_curve;
		    }
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " pole-seeded seam regeneration for trim "
			    << (seam ? seam->m_trim_index : -1) << " rejected: "
			    << (regeneration_failure.empty() ? "endpoint mismatch" :
				regeneration_failure) << std::endl;
		}

		bool change_previous = previous_uv.DistanceTo(common) >
		    ON_ZERO_TOLERANCE;
		bool change_next = next_uv.DistanceTo(common) > ON_ZERO_TOLERANCE;
		/* A singular pcurve must lie on the exact collapsed boundary.  Merely
		 * changing one endpoint can leave a nearly-isoparametric curve carrying
		 * a stale W/E/S/N flag.  Project the complete parameter curve onto its
		 * proven boundary while preserving its varying coordinate and domain.
		 * Every point has the same model-space lift at a true surface pole; the
		 * validation below nevertheless proves the projection stays within the
		 * locally established topology tolerance. */
		const auto singular_candidate = [surface, &boundary_parameter](
		    const ON_BrepTrim &original, const ON_3dPoint &endpoint,
		    bool replace_start) -> ON_Curve * {
		    int fixed_axis = -1;
		    double fixed_parameter = ON_UNSET_VALUE;
		    if (original.m_type != ON_BrepTrim::singular ||
			    !boundary_parameter(surface, original.m_iso, &fixed_axis,
				&fixed_parameter))
			return NULL;
		    const ON_Interval domain = original.Domain();
		    const int samples = std::min(4096,
			std::max(64, original.SpanCount() * 8));
		    ON_3dPointArray points;
		    ON_SimpleArray<double> parameters;
		    points.Reserve(samples + 1);
		    parameters.Reserve(samples + 1);
		    for (int sample = 0; sample <= samples; ++sample) {
			if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
			    return NULL;
			const double fraction = static_cast<double>(sample) / samples;
			const double parameter = domain.ParameterAt(fraction);
			ON_3dPoint point = original.PointAt(parameter);
			if ((replace_start && sample == 0) ||
				(!replace_start && sample == samples))
			    point = endpoint;
			point[fixed_axis] = fixed_parameter;
			point.z = 0.0;
			if (points.Count() > 0 && point.DistanceTo(
				points[points.Count() - 1]) <= ON_ZERO_TOLERANCE) {
			    if (sample == samples) {
				points[points.Count() - 1] = point;
				parameters[parameters.Count() - 1] = parameter;
			    }
			    continue;
			}
			points.Append(point);
			parameters.Append(parameter);
		    }
		    if (points.Count() < 2 || points.Count() != parameters.Count())
			return NULL;
		    parameters[0] = domain.Min();
		    parameters[parameters.Count() - 1] = domain.Max();
		    ON_PolylineCurve *candidate = new ON_PolylineCurve(points, parameters);
		    if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
			delete candidate;
			return NULL;
		    }
		    return candidate;
		};
		std::string previous_construction_failure;
		std::string next_construction_failure;
		ON_Curve *previous_candidate = change_previous ?
		    (previous->m_type == ON_BrepTrim::singular ?
			singular_candidate(*previous, common, false) :
		     previous->m_type == ON_BrepTrim::seam ?
			seam_endpoint_candidate(surface, *previous, common, false,
			    &previous_construction_failure) :
			previous->DuplicateCurve()) : NULL;
		ON_Curve *next_candidate = change_next ?
		    (next->m_type == ON_BrepTrim::singular ?
			singular_candidate(*next, common, true) :
		     next->m_type == ON_BrepTrim::seam ?
			seam_endpoint_candidate(surface, *next, common, true,
			    &next_construction_failure) :
			next->DuplicateCurve()) : NULL;
		const bool previous_constructed = !change_previous ||
		    (previous_candidate &&
		     (previous->m_type == ON_BrepTrim::singular ||
			previous->m_type == ON_BrepTrim::seam ||
			previous_candidate->SetEndPoint(common)) &&
		     previous_candidate->ChangeDimension(2) &&
		     previous_candidate->IsValid() &&
		     previous_candidate->PointAtEnd().DistanceTo(common) <=
			ON_ZERO_TOLERANCE);
		const bool next_constructed = !change_next ||
		    (next_candidate &&
		     (next->m_type == ON_BrepTrim::singular ||
			next->m_type == ON_BrepTrim::seam ||
			next_candidate->SetStartPoint(common)) &&
		     next_candidate->ChangeDimension(2) &&
		     next_candidate->IsValid() &&
		     next_candidate->PointAtStart().DistanceTo(common) <=
			ON_ZERO_TOLERANCE);
		if (!change_previous && !change_next) {
		    delete previous_candidate;
		    delete next_candidate;
		    continue;
		}

		const auto validates = [brep, surface, &join_tolerance, wrapper,
		    &boundary_parameter](
		    const ON_BrepTrim &original,
		    const ON_Curve &candidate, std::string *failure) {
		    if (failure)
			failure->clear();
		    /* Distinct topology vertices require an open parameter-space
		     * curve even when their model-space points coincide within the
		     * source tolerance.  Endpoint reconciliation must not collapse
		     * such a trim into a closed c2 curve: OpenNURBS rejects that
		     * representation, and the separate STEP vertex identities remain
		     * authoritative. */
		    if (original.m_vi[0] != original.m_vi[1] &&
			candidate.IsClosed()) {
			if (failure)
			    *failure = "candidate closed between distinct topology vertices";
			return false;
		    }
		    const ON_Interval original_domain = original.Domain();
		    const ON_Interval candidate_domain = candidate.Domain();
		    if (!original_domain.IsIncreasing() ||
			    !candidate_domain.IsIncreasing()) {
			if (failure)
			    *failure = "original or candidate parameter domain was invalid";
			return false;
		    }
		    if (original.m_type == ON_BrepTrim::seam) {
			const ON_Surface::ISO derived = surface->IsIsoparametric(
			    candidate, &candidate_domain);
			if (derived != original.m_iso) {
			    if (failure)
				*failure = "seam iso classification changed";
			    return false;
			}
		    } else if (original.m_type == ON_BrepTrim::singular) {
			/* IsIsoparametric() is deliberately geometric.  On a
			 * collapsed surface side it may report not_iso (or the
			 * other direction) even when every UV lies exactly on the
			 * required native boundary.  For singular topology the
			 * fixed boundary coordinate is the authoritative
			 * parameter-space proof; the dense model-space loop below
			 * independently requires every lift to remain at the STEP
			 * vertex. */
			int fixed_axis = -1;
			double fixed_parameter = ON_UNSET_VALUE;
			if (!boundary_parameter(surface, original.m_iso,
				&fixed_axis, &fixed_parameter)) {
			    if (failure)
				*failure =
				    "singular trim had no native boundary ISO";
			    return false;
			}
			const double parameter_tolerance =
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale *
			    std::max(1.0, fabs(fixed_parameter));
			const int boundary_samples = std::min(4096,
			    std::max(64, candidate.SpanCount() * 8));
			for (int sample = 0; sample <= boundary_samples; ++sample) {
			    const ON_3dPoint uv = candidate.PointAt(
				candidate_domain.ParameterAt(
				    static_cast<double>(sample) /
				    boundary_samples));
			    if (!uv.IsValid() ||
				    fabs(uv[fixed_axis] - fixed_parameter) >
					parameter_tolerance) {
				if (failure)
				    *failure =
					"singular pcurve left its native boundary";
				return false;
			    }
			}
		    }
		    ON_NurbsCurve edge_nurbs;
		    const ON_BrepEdge *edge = original.Edge();
		    const double edge_tolerance = edge ?
			std::max(LocalUnits::tolerance,
			    std::max(edge->m_tolerance,
				std::max(original.m_tolerance[0],
				    original.m_tolerance[1]))) : join_tolerance;
		    if (edge && !edge->GetNurbForm(edge_nurbs)) {
			if (failure)
			    *failure = "edge NURBS conversion failed";
			return false;
		    }
		    const int validation_spans = std::max(original.SpanCount(),
			candidate.SpanCount());
		    const int samples = std::min(4096,
			std::max(64, validation_spans * 8));
		    for (int sample = 0; sample <= samples; ++sample) {
			if ((sample & 63) == 0 &&
				brlcad::PullbackWorkCancelled()) {
			    if (failure)
				*failure = "endpoint candidate validation was cancelled";
			    return false;
			}
			const double fraction = static_cast<double>(sample) / samples;
			const double original_parameter =
			    original_domain.ParameterAt(fraction);
			const double candidate_parameter =
			    candidate_domain.ParameterAt(fraction);
			const ON_3dPoint original_uv =
			    original.PointAt(original_parameter);
			const ON_3dPoint candidate_uv =
			    candidate.PointAt(candidate_parameter);
			const ON_3dPoint original_lift =
			    closed_surface_point_at(surface, original_uv);
			const ON_3dPoint candidate_lift =
			    closed_surface_point_at(surface, candidate_uv);
			if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
				original_lift.DistanceTo(candidate_lift) >
				    join_tolerance) {
			    if (failure)
				*failure = "surface lift changed at sample " +
				    std::to_string(sample) + " by " +
				    std::to_string(original_lift.DistanceTo(candidate_lift));
			    return false;
			}
			if (edge) {
			    const ON_3dPoint corresponding_edge_point = edge->PointAt(
				edge->Domain().ParameterAt(original.m_bRev3d ?
				    1.0 - fraction : fraction));
			    double edge_distance = corresponding_edge_point.IsValid() ?
				candidate_lift.DistanceTo(corresponding_edge_point) : DBL_MAX;
			    /* Endpoint candidates preserve the trim's parameterization in
			     * the overwhelmingly common case.  The corresponding exact edge
			     * point is therefore a sufficient, stronger validation and avoids
			     * thousands of expensive global NURBS closest-point searches on
			     * large solids.  Fall back to the locus query only when that direct
			     * correspondence does not already prove the candidate. */
			    /* A closest-locus match is sufficient for an interior sample
			     * when the STEP pcurve legitimately reparameterizes its edge,
			     * but it cannot establish directed endpoint correspondence.
			     * Allowing the global closest point at sample 0 or N can attach
			     * an open trim to the opposite end (or an unrelated interior
			     * point) of the right 3-D edge while still passing every locus
			     * check.  OpenNURBS then reports the cap/band as disconnected.
			     * Require the authoritative directed edge endpoint at both ends;
			     * only interior samples may use the locus fallback. */
			    const bool endpoint_sample = sample == 0 || sample == samples;
			    /* Endpoint reconciliation is authorized by the complete local
			     * topology proof above.  In safe mode that proof can carry a
			     * densely measured vertex/neighbor tolerance larger than this
			     * trim's own edge tolerance.  Use it only at the one edited
			     * endpoint; interior samples remain constrained by the original
			     * trim/edge tolerance.  --exact never widens the endpoint bound. */
			    const double sample_tolerance = endpoint_sample &&
				!wrapper->ImportOptions().exact ?
				std::max(edge_tolerance, join_tolerance) : edge_tolerance;
			    if (edge_distance > sample_tolerance && sample > 0 &&
				    sample < samples) {
				double edge_parameter = 0.0;
				if (ON_NurbsCurve_GetClosestPoint(&edge_parameter,
					&edge_nurbs, candidate_lift))
				    edge_distance = std::min(edge_distance,
					candidate_lift.DistanceTo(
					    edge_nurbs.PointAt(edge_parameter)));
				/* The candidate differs from an already accepted source
				 * pcurve only by this bounded endpoint edit.  A global
				 * closest-point solve seeded from the candidate can converge to
				 * the wrong branch of a closed or high-curvature edge.  Reuse
				 * the original lift's independently solved edge point as a
				 * second exact-locus witness; this changes no tolerance. */
				if (edge_distance > sample_tolerance) {
				    double original_edge_parameter = 0.0;
				    if (ON_NurbsCurve_GetClosestPoint(
					    &original_edge_parameter, &edge_nurbs,
					    original_lift)) {
					const ON_3dPoint original_edge_point =
					    edge_nurbs.PointAt(original_edge_parameter);
					if (original_edge_point.IsValid() &&
						original_lift.DistanceTo(original_edge_point) <=
						    edge_tolerance)
					    edge_distance = std::min(edge_distance,
						candidate_lift.DistanceTo(original_edge_point));
				    }
				}
			    }
			    if (edge_distance > sample_tolerance) {
				if (failure)
				    *failure = (endpoint_sample ?
					"directed edge endpoint distance at sample " :
					"edge distance at sample ") +
					std::to_string(sample) + " was " +
					std::to_string(edge_distance);
				return false;
			    }
			} else if (original.m_vi[0] >= 0 &&
				original.m_vi[0] < brep->m_V.Count() &&
				candidate_lift.DistanceTo(
				    brep->m_V[original.m_vi[0]].point) >
				    join_tolerance) {
			    if (failure)
				*failure = "singular trim lift left its vertex";
			    return false;
			}
		    }
		    return true;
		};
		/* A shared STEP vertex can be farther apart on its two independently
		 * supplied edge/surface associations than either edge's local
		 * tolerance, while both associations remain inside the already
		 * proven join tolerance.  An endpoint-local candidate then needs a
		 * correspondingly local OpenNURBS trim tolerance for the short
		 * transition into that common UV.  Measure the complete candidate;
		 * retain the exact 3-D edge and surface; and authorize only a
		 * scale-bounded tolerance no larger than the join proof. */
		const auto verified_endpoint_candidate_tolerance =
		    [brep, surface, wrapper, entity_id, &entity_type, item_scale,
		     record_changes](
			ON_BrepTrim &trim, const ON_Curve &candidate,
			double candidate_join_tolerance) {
		    if (wrapper->ImportOptions().exact ||
			    wrapper->ImportOptions().repair !=
				brlcad::step::RepairMode::Safe ||
			    !(candidate_join_tolerance > 0.0))
			return false;
		    ON_BrepEdge *edge = trim.Edge();
		    ON_NurbsCurve edge_nurbs;
		    if (!edge || !edge->GetNurbForm(edge_nurbs))
			return false;
		    const ON_Interval trim_domain = trim.Domain();
		    const ON_Interval candidate_domain = candidate.Domain();
		    const ON_Interval edge_domain = edge_nurbs.Domain();
		    if (!trim_domain.IsIncreasing() ||
			    !candidate_domain.IsIncreasing() ||
			    !edge_domain.IsIncreasing())
			return false;

		    const double existing_tolerance = std::max(
			LocalUnits::tolerance,
			std::max(edge->m_tolerance,
			    std::max(trim.m_tolerance[0],
				trim.m_tolerance[1])));
		    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
		    const double edge_scale = edge_bounds.IsValid() ?
			edge_bounds.Diagonal().Length() : 0.0;
		    const double scale_limit = std::max(existing_tolerance,
			std::max(edge_scale *
				kRegenerationMaximumRelativeMismatch,
			    item_scale *
				kRegenerationMaximumRelativeItemMismatch));
		    const double allowed_tolerance =
			std::min(candidate_join_tolerance, scale_limit);
		    if (!(allowed_tolerance > existing_tolerance))
			return false;

		    const int validation_spans = std::max(trim.SpanCount(),
			candidate.SpanCount());
		    const int samples = std::min(4096,
			std::max(64, validation_spans * 8));
		    double measured = 0.0;
		    for (int sample = 0; sample <= samples; ++sample) {
			if ((sample & 63) == 0 &&
				brlcad::PullbackWorkCancelled())
			    return false;
			const double fraction =
			    static_cast<double>(sample) / samples;
			const ON_3dPoint source_uv = trim.PointAt(
			    trim_domain.ParameterAt(fraction));
			const ON_3dPoint candidate_uv = candidate.PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint source_lift =
			    closed_surface_point_at(surface, source_uv);
			const ON_3dPoint candidate_lift =
			    closed_surface_point_at(surface, candidate_uv);
			if (!source_lift.IsValid() || !candidate_lift.IsValid() ||
				source_lift.DistanceTo(candidate_lift) >
				    candidate_join_tolerance)
			    return false;
			const ON_3dPoint corresponding = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(trim.m_bRev3d ?
				1.0 - fraction : fraction));
			if (!corresponding.IsValid())
			    return false;
			double distance =
			    candidate_lift.DistanceTo(corresponding);
			if (distance > std::max(existing_tolerance, measured) &&
				sample > 0 && sample < samples) {
			    double edge_parameter = 0.0;
			    if (!ON_NurbsCurve_GetClosestPoint(&edge_parameter,
				    &edge_nurbs, candidate_lift))
				return false;
			    distance = std::min(distance,
				candidate_lift.DistanceTo(
				    edge_nurbs.PointAt(edge_parameter)));
			}
			measured = std::max(measured, distance);
			if (measured * kRegenerationToleranceSafety >
				allowed_tolerance)
			    return false;
		    }
		    if (!(measured > existing_tolerance))
			return false;
		    const double adjusted =
			measured * kRegenerationToleranceSafety;
		    if (!(adjusted <= allowed_tolerance))
			return false;

		    edge->m_tolerance =
			std::max(edge->m_tolerance, adjusted);
		    trim.m_tolerance[0] =
			std::max(trim.m_tolerance[0], adjusted);
		    trim.m_tolerance[1] =
			std::max(trim.m_tolerance[1], adjusted);
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": trim "
			    << trim.m_trim_index << "/STEP edge "
			    << edge->m_edge_user.i
			    << " endpoint-local candidate mismatch=" << measured
			    << " existing tolerance=" << existing_tolerance
			    << " adjusted tolerance=" << adjusted
			    << " join/scale bounds=" << candidate_join_tolerance << '/'
			    << scale_limit << std::endl;
		    if (record_changes) {
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning,
			    entity_id, entity_type, "trim_pcurve",
			    "independent edge/surface associations at a shared "
			    "STEP vertex exceeded one trim tolerance; used a "
			    "densely measured endpoint-local OpenNURBS tolerance");
			wrapper->RecordRepair(entity_id, entity_type,
			    "trim_pcurve",
			    "adjusted one endpoint-local trim tolerance after "
			    "complete candidate validation");
		    }
		    return true;
		};
		const auto localized_candidate = [](const ON_BrepTrim &original,
		    const ON_3dPoint &endpoint, bool replace_start,
		    int sample_count) -> ON_Curve * {
		    if (sample_count < 2)
			return NULL;
		    const ON_Interval domain = original.Domain();
		    ON_3dPointArray points;
		    ON_SimpleArray<double> parameters;
		    points.Reserve(sample_count + 1);
		    parameters.Reserve(sample_count + 1);
		    for (int sample = 0; sample <= sample_count; ++sample) {
			if ((sample & 63) == 0 &&
				brlcad::PullbackWorkCancelled())
			    return NULL;
			const double fraction = static_cast<double>(sample) / sample_count;
			const double parameter = domain.ParameterAt(fraction);
			ON_3dPoint point = original.PointAt(parameter);
			if ((replace_start && sample == 0) ||
				(!replace_start && sample == sample_count))
			    point = endpoint;
			point.z = 0.0;
			if (points.Count() > 0 &&
				point.DistanceTo(points[points.Count() - 1]) <=
				    ON_ZERO_TOLERANCE) {
			    if (sample == sample_count) {
				points[points.Count() - 1] = point;
				parameters[parameters.Count() - 1] = parameter;
			    }
			    continue;
			}
			points.Append(point);
			parameters.Append(parameter);
		    }
		    if (points.Count() < 2 || points.Count() != parameters.Count())
			return NULL;
		    parameters[0] = domain.Min();
		    parameters[parameters.Count() - 1] = domain.Max();
		    ON_PolylineCurve *candidate = new ON_PolylineCurve(points, parameters);
		    if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
			delete candidate;
			return NULL;
		    }
		    return candidate;
		};
		std::string previous_failure = previous_constructed ? std::string() :
		    (previous_construction_failure.empty() ?
			"endpoint construction failed" :
			previous_construction_failure);
		std::string next_failure = next_constructed ? std::string() :
		    (next_construction_failure.empty() ?
			"endpoint construction failed" :
			next_construction_failure);
		bool previous_valid = previous_constructed && (!change_previous ||
		    validates(*previous, *previous_candidate, &previous_failure));
		bool next_valid = next_constructed && (!change_next ||
		    validates(*next, *next_candidate, &next_failure));
		if (!previous_valid || !next_valid) {
		    /* A midpoint is not always the best parameter-space join near a
		     * singularity or strongly nonlinear edge.  Prefer either supplied
		     * endpoint when moving only the other curve remains within the exact
		     * surface and edge bounds. */
		    for (int endpoint_choice = 0;
			    (!previous_valid || !next_valid) && endpoint_choice < 2;
			    ++endpoint_choice) {
			if (brlcad::PullbackWorkCancelled()) {
			    delete previous_candidate;
			    delete next_candidate;
			    return;
			}
			const ON_3dPoint alternative_common = endpoint_choice == 0 ?
			    previous_uv : next_uv;
			if (alternative_common.DistanceTo(common) <= ON_ZERO_TOLERANCE)
			    continue;
			const bool alternative_change_previous =
			    previous_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			const bool alternative_change_next =
			    next_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			if ((previous->m_type == ON_BrepTrim::singular &&
				alternative_change_previous) ||
				(next->m_type == ON_BrepTrim::singular &&
				 alternative_change_next))
			    continue;
			ON_Curve *previous_alternative = alternative_change_previous ?
			    (previous->m_type == ON_BrepTrim::seam ?
				seam_endpoint_candidate(surface, *previous,
				    alternative_common, false) :
				previous->DuplicateCurve()) : NULL;
			ON_Curve *next_alternative = alternative_change_next ?
			    (next->m_type == ON_BrepTrim::seam ?
				seam_endpoint_candidate(surface, *next,
				    alternative_common, true) :
				next->DuplicateCurve()) : NULL;
			const bool alternative_previous_constructed =
			    !alternative_change_previous ||
			    (previous_alternative &&
			     (previous->m_type == ON_BrepTrim::seam ||
				previous_alternative->SetEndPoint(
				    alternative_common)) &&
			     previous_alternative->ChangeDimension(2) &&
			     previous_alternative->IsValid() &&
			     previous_alternative->PointAtEnd().DistanceTo(
				alternative_common) <= ON_ZERO_TOLERANCE);
			const bool alternative_next_constructed =
			    !alternative_change_next ||
			    (next_alternative &&
			     (next->m_type == ON_BrepTrim::seam ||
				next_alternative->SetStartPoint(
				    alternative_common)) &&
			     next_alternative->ChangeDimension(2) &&
			     next_alternative->IsValid() &&
			     next_alternative->PointAtStart().DistanceTo(
				alternative_common) <= ON_ZERO_TOLERANCE);
			std::string previous_alternative_failure;
			std::string next_alternative_failure;
			bool previous_alternative_valid =
			    alternative_previous_constructed &&
			    (!alternative_change_previous || validates(*previous,
				*previous_alternative, &previous_alternative_failure));
			bool next_alternative_valid =
			    alternative_next_constructed &&
			    (!alternative_change_next || validates(*next,
				*next_alternative, &next_alternative_failure));
		    for (int endpoint_samples = 64;
			    (!previous_alternative_valid || !next_alternative_valid) &&
				endpoint_samples <= kMaximumLocalizedEndpointSamples;
				endpoint_samples *= 2) {
			    if (brlcad::PullbackWorkCancelled())
				break;
			    delete previous_alternative;
			    delete next_alternative;
			    previous_alternative = alternative_change_previous ?
				localized_candidate(*previous, alternative_common, false,
				    endpoint_samples) : NULL;
			    next_alternative = alternative_change_next ?
				localized_candidate(*next, alternative_common, true,
				    endpoint_samples) : NULL;
			    previous_alternative_failure.clear();
			    next_alternative_failure.clear();
			    previous_alternative_valid = !alternative_change_previous ||
				(previous_alternative && validates(*previous,
				    *previous_alternative, &previous_alternative_failure));
			    next_alternative_valid = !alternative_change_next ||
				(next_alternative && validates(*next, *next_alternative,
				    &next_alternative_failure));
			}
			if (previous_alternative_valid && next_alternative_valid &&
				(alternative_change_previous || alternative_change_next)) {
			    delete previous_candidate;
			    delete next_candidate;
			    previous_candidate = previous_alternative;
			    next_candidate = next_alternative;
			    common = alternative_common;
			    change_previous = alternative_change_previous;
			    change_next = alternative_change_next;
			    previous_valid = true;
			    next_valid = true;
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " selected existing endpoint for trims "
				    << previous->m_trim_index << '/' << next->m_trim_index
				    << std::endl;
			} else {
			    delete previous_alternative;
			    delete next_alternative;
			    previous_failure = previous_alternative_failure;
			    next_failure = next_alternative_failure;
			}
		    }
		    bool localized = previous_valid && next_valid;
		    for (int samples = 64; !localized &&
			    samples <= kMaximumLocalizedEndpointSamples; samples *= 2) {
			if (brlcad::PullbackWorkCancelled()) {
			    delete previous_candidate;
			    delete next_candidate;
			    return;
			}
			ON_Curve *previous_alternative = change_previous ?
			    localized_candidate(*previous, common, false, samples) : NULL;
			ON_Curve *next_alternative = change_next ?
			    localized_candidate(*next, common, true, samples) : NULL;
			std::string previous_alternative_failure;
			std::string next_alternative_failure;
			previous_valid = !change_previous || (previous_alternative &&
			    validates(*previous, *previous_alternative,
				&previous_alternative_failure));
			next_valid = !change_next || (next_alternative && validates(*next,
			    *next_alternative, &next_alternative_failure));
			if (previous_valid && next_valid) {
			    delete previous_candidate;
			    delete next_candidate;
			    previous_candidate = previous_alternative;
			    next_candidate = next_alternative;
			    localized = true;
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " localized endpoint repair for trims "
				    << previous->m_trim_index << '/' << next->m_trim_index
				    << " with " << samples << " pcurve samples" << std::endl;
			} else {
			    delete previous_alternative;
			    delete next_alternative;
			    previous_failure = previous_alternative_failure;
			    next_failure = next_alternative_failure;
			}
		    }
		    if (!localized) {
			/* Endpoint repair must not preserve a supplied pcurve that is
			 * already farther from its exact 3D edge than the model
			 * uncertainty.  Regenerate that non-seam pcurve from the edge,
			 * retaining its shared endpoints, and retry this join on the next
			 * bounded pass. */
			const auto edge_distance_failure = [](const std::string &failure) {
			    return failure.find("edge distance") !=
				std::string::npos ||
				failure.find("directed edge endpoint distance") !=
				    std::string::npos;
			};
			std::vector<ON_BrepTrim *> invalid_trims;
			if (!previous_valid &&
				previous->m_type != ON_BrepTrim::seam &&
				edge_distance_failure(previous_failure))
			    invalid_trims.push_back(previous);
			if (!next_valid && next->m_type != ON_BrepTrim::seam &&
				edge_distance_failure(next_failure))
			    invalid_trims.push_back(next);
			bool regenerated = false;
			ON_BrepTrim *regenerated_trim = NULL;
			std::string regeneration_failure;
			for (std::vector<ON_BrepTrim *>::iterator invalid =
				invalid_trims.begin();
				!regenerated && invalid != invalid_trims.end();
				++invalid) {
			    ON_BrepTrim *invalid_trim = *invalid;
			    if (!attempted_edge_regeneration.insert(
				    invalid_trim->m_trim_index).second)
				continue;
			    ON_BrepEdge *invalid_edge = invalid_trim->Edge();
			    ON_NurbsCurve edge_nurbs;
			    double regeneration_tolerance =
				LocalUnits::tolerance;
			    if (invalid_edge)
				regeneration_tolerance = std::max(
				    regeneration_tolerance,
				    invalid_edge->m_tolerance);
			    regeneration_tolerance = std::max(
				regeneration_tolerance,
				std::max(invalid_trim->m_tolerance[0],
				    invalid_trim->m_tolerance[1]));
			    if (invalid_edge &&
				    invalid_edge->GetNurbForm(edge_nurbs)) {
				/* Regenerating from the supplied pcurve without fixing
				 * the failed join can faithfully reproduce the same bad
				 * endpoint and report success.  Preserve the trim's
				 * already-connected opposite endpoint, constrain this
				 * endpoint to the loop's validated common UV, and drive
				 * the interior from the immutable 3-D STEP edge.  The
				 * pullback helper is transactional: it installs nothing
				 * unless the complete candidate satisfies its dense
				 * surface/edge validation. */
				ON_3dPoint required_start =
				    invalid_trim->PointAtStart();
				ON_3dPoint required_end =
				    invalid_trim->PointAtEnd();
				if (invalid_trim == previous)
				    required_end = common;
				else if (invalid_trim == next)
				    required_start = common;
				ON_Curve *regenerated_curve = NULL;
				const bool generated = regenerate_trim_polyline(brep,
				    *invalid_trim,
				    surface, edge_nurbs, regeneration_tolerance,
				    &regeneration_failure, NULL, &required_start,
				    &required_end, true, wrapper, true,
				    &regenerated_curve);
				/* The pullback helper's adaptive proof is optimized for
				 * its sampled polyline.  Endpoint reconciliation applies
				 * the stricter final loop validator, including its dense
				 * near-end edge-locus check.  Validate before installation
				 * so a source-driven fallback cannot replace the trim and
				 * then leave the same join open. */
				std::string candidate_failure;
				const ON_3dPoint generated_join =
				    regenerated_curve ?
				    (invalid_trim == previous ?
					regenerated_curve->PointAtEnd() :
					regenerated_curve->PointAtStart()) :
				    ON_3dPoint::UnsetPoint;
				const bool honored_join = generated_join.IsValid() &&
				    generated_join.DistanceTo(common) <=
					ON_ZERO_TOLERANCE;
				if (generated && regenerated_curve &&
					honored_join &&
					validates(*invalid_trim, *regenerated_curve,
					    &candidate_failure)) {
				    const int c2_index =
					brep->AddTrimCurve(regenerated_curve);
				    if (c2_index >= 0 &&
					    brep->SetTrimCurve(*invalid_trim,
						c2_index)) {
					brep->SetTrimIsoFlags(*invalid_trim);
					regenerated = true;
					regenerated_trim = invalid_trim;
				    } else if (c2_index < 0) {
					delete regenerated_curve;
				    }
				} else {
				    delete regenerated_curve;
				    if (generated && !honored_join)
					candidate_failure =
					    "regenerated curve did not honor the "
					    "required loop endpoint";
				    if (!candidate_failure.empty())
					regeneration_failure =
					    "final endpoint validation: " +
					    candidate_failure;
				}
			    }
			}
			if (regenerated) {
			    record_repair("trim_pcurve",
				"regenerated an invalid pcurve from the exact edge");
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " regenerated trim "
				    << regenerated_trim->m_trim_index
				    << "/STEP edge "
				    << (regenerated_trim->Edge() ?
					regenerated_trim->Edge()->m_edge_user.i : 0)
				    << " from its exact edge before endpoint repair; join="
				    << (regenerated_trim == previous ?
					regenerated_trim->PointAtEnd().x :
					regenerated_trim->PointAtStart().x)
				    << ':'
				    << (regenerated_trim == previous ?
					regenerated_trim->PointAtEnd().y :
					regenerated_trim->PointAtStart().y)
				    << " required=" << common.x << ':' << common.y
				    << std::endl;
			    delete previous_candidate;
			    delete next_candidate;
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			/* Exact pullback has now failed for every invalid side.  The
			 * existing pcurve may still be the exporter's best available
			 * association when the asserted edge and surface do not agree.
			 * Measure that complete association densely, apply only the
			 * scale-bounded local tolerance in safe mode, and revalidate the
			 * endpoint candidate.  This is not a license to hide a wrong
			 * branch: a candidate needing more than the measured existing
			 * source association remains rejected. */
			bool measured_source_tolerance = false;
			for (std::vector<ON_BrepTrim *>::iterator invalid =
				invalid_trims.begin(); invalid != invalid_trims.end();
				++invalid) {
			    ON_BrepTrim *invalid_trim = *invalid;
			    ON_BrepEdge *invalid_edge = invalid_trim->Edge();
			    ON_NurbsCurve edge_nurbs;
			    if (!invalid_edge ||
				    !invalid_edge->GetNurbForm(edge_nurbs))
				continue;
			    const double existing_tolerance = std::max(
				LocalUnits::tolerance,
				std::max(invalid_edge->m_tolerance,
				    std::max(invalid_trim->m_tolerance[0],
					invalid_trim->m_tolerance[1])));
			    const double verified_tolerance =
				verified_source_pcurve_tolerance(*invalid_trim,
				    *invalid_edge, surface, edge_nurbs,
				    existing_tolerance, brep, wrapper, entity_id,
				    entity_type);
			    if (verified_tolerance > existing_tolerance) {
				join_tolerance = std::max(join_tolerance,
				    verified_tolerance);
				measured_source_tolerance = true;
			    }
			}
			if (measured_source_tolerance) {
			    previous_failure.clear();
			    next_failure.clear();
			    previous_valid = !change_previous ||
				(previous_candidate && validates(*previous,
				    *previous_candidate, &previous_failure));
			    next_valid = !change_next ||
				(next_candidate && validates(*next,
				    *next_candidate, &next_failure));
			    localized = previous_valid && next_valid;
			}
			/* Exact-edge regeneration and the existing-source
			 * measurement can both be insufficient when the discrepancy is
			 * confined to reconciling two valid associations at their
			 * shared topology vertex.  Measure the already constructed
			 * endpoint-local candidate itself, bounded by the complete join
			 * proof, then rerun the ordinary dense validator. */
			bool measured_candidate_tolerance = false;
			if (!localized && !previous_valid && previous_candidate)
			    measured_candidate_tolerance =
				verified_endpoint_candidate_tolerance(*previous,
				    *previous_candidate, join_tolerance) ||
				measured_candidate_tolerance;
			if (!localized && !next_valid && next_candidate)
			    measured_candidate_tolerance =
				verified_endpoint_candidate_tolerance(*next,
				    *next_candidate, join_tolerance) ||
				measured_candidate_tolerance;
			if (measured_candidate_tolerance) {
			    previous_failure.clear();
			    next_failure.clear();
			    previous_valid = !change_previous ||
				(previous_candidate && validates(*previous,
				    *previous_candidate, &previous_failure));
			    next_valid = !change_next ||
				(next_candidate && validates(*next,
				    *next_candidate, &next_failure));
			    localized = previous_valid && next_valid;
			}
			if (!localized) {
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " endpoint candidate "
				    << previous->m_trim_index << '/' << next->m_trim_index
				    << " rejected: previous="
				    << (previous_valid ? "valid" : previous_failure)
				    << ", next="
				    << (next_valid ? "valid" : next_failure)
				    << (regeneration_failure.empty() ? "" :
					", exact-edge regeneration=" +
					    regeneration_failure)
				    << std::endl;
			    delete previous_candidate;
			    delete next_candidate;
			    continue;
			}
		    }
		}
		const int previous_c2 = previous_candidate ?
		    brep->AddTrimCurve(previous_candidate) : -1;
		const int next_c2 = next_candidate ? brep->AddTrimCurve(next_candidate) : -1;
		const bool previous_installed = !previous_candidate ||
		    (previous_c2 >= 0 && brep->SetTrimCurve(*previous, previous_c2));
		const bool next_installed = !next_candidate ||
		    (next_c2 >= 0 && brep->SetTrimCurve(*next, next_c2));
		if (!previous_installed || !next_installed) {
		    if (previous_candidate && previous_c2 < 0)
			delete previous_candidate;
		    if (next_candidate && next_c2 < 0)
			delete next_candidate;
		    continue;
		}
		if (previous_candidate) {
		    brep->SetTrimIsoFlags(*previous);
		    if (!wrapper->ImportOptions().exact &&
			    wrapper->ImportOptions().repair ==
				brlcad::step::RepairMode::Safe) {
			previous->m_tolerance[0] = std::max(
			    previous->m_tolerance[0], join_tolerance);
			previous->m_tolerance[1] = std::max(
			    previous->m_tolerance[1], join_tolerance);
		    }
		}
		if (next_candidate) {
		    brep->SetTrimIsoFlags(*next);
		    if (!wrapper->ImportOptions().exact &&
			    wrapper->ImportOptions().repair ==
				brlcad::step::RepairMode::Safe) {
			next->m_tolerance[0] = std::max(
			    next->m_tolerance[0], join_tolerance);
			next->m_tolerance[1] = std::max(
			    next->m_tolerance[1], join_tolerance);
		    }
		}
		record_repair("edge_loop",
		    "snapped adjacent pcurve endpoints within validated edge tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": snapped loop "
			<< li << " trim endpoints " << previous->m_trim_index << '/'
			<< next->m_trim_index << " within tolerance "
			<< join_tolerance << "; installed join "
			<< previous->PointAtEnd().x << ':'
			<< previous->PointAtEnd().y << " -> "
			<< next->PointAtStart().x << ':'
			<< next->PointAtStart().y << std::endl;
		changed = true;
		loop_changed = true;
		continue;
	    }
	    next_dirty[static_cast<size_t>(li)] = loop_changed;
	}
	if (!changed)
	    break;
	dirty.swap(next_dirty);
    }
}


size_t
repair_paired_seam_loop_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_pairs = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &source_loop = brep->m_L[li];
	const ON_BrepFace *source_face = source_loop.Face();
	if (face_source_tags && (!source_face || face_source_tags->find(
		source_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	if (source_loop.TrimCount() < 4)
	    continue;

	std::map<int, std::vector<int> > seam_positions;
	for (int lti = 0; lti < source_loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = source_loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::seam && trim->m_ei >= 0)
		seam_positions[trim->m_ei].push_back(lti);
	}
	for (std::map<int, std::vector<int> >::const_iterator pair =
		seam_positions.begin(); pair != seam_positions.end(); ++pair) {
	    /* A rejected transaction restores the complete B-Rep below.  That
	     * assignment invalidates every reference and pointer into its arrays,
	     * even though the restored topology has the same indices.  Reacquire
	     * the loop, face, and surface for each pair so a later pair never uses
	     * storage owned by the discarded pre-rollback B-Rep. */
	    if (li < 0 || li >= brep->m_L.Count())
		break;
	    ON_BrepLoop &loop = brep->m_L[li];
	    if (loop.m_fi < 0 || loop.m_fi >= brep->m_F.Count())
		continue;
	    const ON_BrepFace &face = brep->m_F[loop.m_fi];
	    if (face.m_si < 0 || face.m_si >= brep->m_S.Count())
		continue;
	    const ON_Surface *surface = brep->m_S[face.m_si];
	    if (!surface)
		continue;
	    if (pair->second.size() != 2 || pair->first < 0 ||
		    pair->first >= brep->m_E.Count())
		continue;
	    ON_BrepEdge &edge = brep->m_E[pair->first];
	    ON_NurbsCurve edge_nurbs;
	    /* The unique seam pair is established by its two positions in this
	     * loop.  The shared edge may legitimately have additional uses on
	     * other faces, so do not require it to have exactly two global uses. */
	    if (edge.m_ti.Count() < 2 || !edge.GetNurbForm(edge_nurbs))
		continue;

	    struct Candidate {
		int trim_index;
		ON_Surface::ISO iso;
		std::unique_ptr<ON_Curve> curve;
	    };
	    std::vector<Candidate> candidates;
	    std::map<int, std::unique_ptr<ON_Curve> > adjacent_candidates;
	    bool complete = true;
	    bool needs_repair = false;
	    std::string rejection_reason;
	    for (std::vector<int>::const_iterator position = pair->second.begin();
		    position != pair->second.end(); ++position) {
		ON_BrepTrim *trim = loop.Trim(*position);
		ON_BrepTrim *previous = loop.Trim((*position + loop.TrimCount() - 1) %
		    loop.TrimCount());
		ON_BrepTrim *next = loop.Trim((*position + 1) % loop.TrimCount());
		if (!trim || !previous || !next ||
			previous->m_vi[1] != trim->m_vi[0] ||
			trim->m_vi[1] != next->m_vi[0]) {
		    rejection_reason = "seam/adjacent topology vertices did not agree";
		    complete = false;
		    break;
		}
		const ON_3dPoint start = previous->PointAtEnd();
		const ON_3dPoint end = next->PointAtStart();
		if (!start.IsValid() || !end.IsValid()) {
		    rejection_reason = "an adjacent endpoint was invalid";
		    complete = false;
		    break;
		}
		needs_repair = needs_repair ||
		    trim->PointAtStart().DistanceTo(start) > ON_ZERO_TOLERANCE ||
		    trim->PointAtEnd().DistanceTo(end) > ON_ZERO_TOLERANCE;

		/* A preceding periodic branch solve can translate one member of a
		 * correct W/E or S/N seam pair by a complete surface period.  Its
		 * m_iso then describes that translated curve, not the side required by
		 * the completed loop.  Infer the final side from both exact adjacent
		 * endpoints instead.  This is deliberately stricter than choosing the
		 * nearest side from the seam itself: both endpoints must already be on
		 * one closed-domain boundary, and both replacements are committed only
		 * after the complete pair validates against its shared 3-D edge. */
		int fixed_direction = -1;
		int boundary_side = -1;
		double boundary = ON_UNSET_VALUE;
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const ON_Interval surface_domain = surface->Domain(direction);
		    if (!surface_domain.IsIncreasing())
			continue;
		    const double parameter_tolerance =
			wrapper->ImportOptions().exact ? ON_ZERO_TOLERANCE :
			std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    surface_domain.Length() * 1.0e-4);
		    for (int side = 0; side < 2; ++side) {
			if (fabs(start[direction] - surface_domain[side]) >
				parameter_tolerance ||
				fabs(end[direction] - surface_domain[side]) >
				parameter_tolerance)
			    continue;
			if (fixed_direction >= 0) {
			    fixed_direction = -1;
			    boundary_side = -1;
			    break;
			}
			fixed_direction = direction;
			boundary_side = side;
			boundary = surface_domain[side];
		    }
		    if (fixed_direction < 0 && boundary_side < 0)
			continue;
		}
		if (fixed_direction < 0 || boundary_side < 0) {
		    rejection_reason = "adjacent endpoints did not prove one periodic boundary";
		    complete = false;
		    break;
		}

		ON_3dPoint candidate_start = start;
		ON_3dPoint candidate_end = end;
		candidate_start[fixed_direction] = boundary;
		candidate_end[fixed_direction] = boundary;
		const ON_Surface::ISO candidate_iso = fixed_direction == 0 ?
		    (boundary_side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		    (boundary_side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		std::unique_ptr<ON_Curve> candidate(new ON_LineCurve(candidate_start,
		    candidate_end));
		if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
		    rejection_reason = "boundary seam candidate was invalid";
		    complete = false;
		    break;
		}
		const ON_Interval candidate_domain = candidate->Domain();
		const ON_Surface::ISO derived_candidate_iso =
		    surface->IsIsoparametric(*candidate, &candidate_domain);
		if (derived_candidate_iso != candidate_iso) {
		    rejection_reason = "boundary seam candidate derived ISO " +
			std::to_string(static_cast<int>(derived_candidate_iso)) +
			" instead of " +
			std::to_string(static_cast<int>(candidate_iso));
		    complete = false;
		    break;
		}
		/* Project the adjacent ordinary endpoints by the same tiny amount as
		 * the inferred boundary.  Otherwise the new exact boundary seam would
		 * still be separated from an exporter-supplied endpoint a few parameter
		 * ulps outside the native domain.  Accumulate both ends when one ordinary
		 * trim spans the complete band between the two seam uses. */
		const ON_BrepTrim *loop_adjacent[2] = {previous, next};
		const ON_3dPoint adjacent_endpoint[2] = {
		    candidate_start, candidate_end
		};
		for (int side = 0; side < 2; ++side) {
		    if (!loop_adjacent[side] ||
			loop_adjacent[side]->m_type == ON_BrepTrim::seam ||
			loop_adjacent[side]->m_type == ON_BrepTrim::singular) {
			rejection_reason = "a seam neighbor was not an ordinary trim";
			complete = false;
			break;
		    }
		    const int adjacent_index = loop_adjacent[side]->m_trim_index;
		    std::unique_ptr<ON_Curve> &adjacent_candidate =
			adjacent_candidates[adjacent_index];
		    if (!adjacent_candidate)
			adjacent_candidate.reset(loop_adjacent[side]->DuplicateCurve());
		    const bool endpoint_set = adjacent_candidate && (side == 0 ?
			adjacent_candidate->SetEndPoint(adjacent_endpoint[side]) :
			adjacent_candidate->SetStartPoint(adjacent_endpoint[side]));
		    if (!endpoint_set || !adjacent_candidate->ChangeDimension(2) ||
			!adjacent_candidate->IsValid()) {
			rejection_reason = "an adjacent endpoint candidate was invalid";
			complete = false;
			break;
		    }
		    if (loop_adjacent[side]->m_vi[0] !=
			    loop_adjacent[side]->m_vi[1] &&
			    adjacent_candidate->IsClosed()) {
			rejection_reason =
			    "an adjacent endpoint candidate closed between "
			    "distinct STEP topology vertices";
			complete = false;
			break;
		    }
		}
		if (!complete)
		    break;

		double tolerance = std::max(LocalUnits::tolerance,
		    edge.m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		/* Neighboring source edges define the loop join which this candidate
		 * is repairing, so their already measured tolerances may be used to
		 * discover a compatible endpoint.  Acceptance is still
		 * transactional: FaceTrimValidationRegressed separately proves
		 * that the resulting directed pcurves did not move away from their
		 * own 3-D edges, which is the guard needed by the Panzer VII case. */
		for (const ON_BrepTrim *adjacent : {previous, next}) {
		    if (adjacent->Edge())
			tolerance = std::max(tolerance,
			    adjacent->Edge()->m_tolerance);
		    tolerance = std::max(tolerance,
			std::max(adjacent->m_tolerance[0],
			    adjacent->m_tolerance[1]));
		}
		if (trim->m_vi[0] >= 0 && trim->m_vi[0] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[0]].m_tolerance);
		if (trim->m_vi[1] >= 0 && trim->m_vi[1] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[1]].m_tolerance);

		const ON_Interval edge_domain = edge_nurbs.Domain();
		for (int sample = 0; complete &&
			sample <= kPcurveLocusScreeningSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kPcurveLocusScreeningSegments;
		    const ON_3dPoint uv = candidate->PointAt(
			candidate_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const double edge_fraction = trim->m_bRev3d ?
			1.0 - fraction : fraction;
		    const ON_3dPoint edge_point = edge_nurbs.PointAt(
			edge_domain.ParameterAt(edge_fraction));
		    double distance = lift.IsValid() && edge_point.IsValid() ?
			lift.DistanceTo(edge_point) : DBL_MAX;
		    if (distance > tolerance && lift.IsValid()) {
			double edge_parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&edge_parameter,
				&edge_nurbs, lift))
			    distance = std::min(distance, lift.DistanceTo(
				edge_nurbs.PointAt(edge_parameter)));
		    }
		    if (distance > tolerance)
			rejection_reason = "boundary seam candidate left its exact edge";
		    if (distance > tolerance)
			complete = false;
		}
		if (!complete)
		    break;
		candidates.push_back({trim->m_trim_index, candidate_iso,
		    std::move(candidate)});
	    }
	    /* The adjacent endpoint projection is admissible only if every complete
	     * curve remains within its already established source tolerance and on
	     * the exact 3-D STEP edge.  This supplies the same proof as the ordinary
	     * endpoint repair, but validates the coupled seam/neighbor edit as one
	     * transaction. */
	    for (std::map<int, std::unique_ptr<ON_Curve> >::const_iterator adjacent =
		    adjacent_candidates.begin(); complete &&
		    adjacent != adjacent_candidates.end(); ++adjacent) {
		if (adjacent->first < 0 || adjacent->first >= brep->m_T.Count() ||
			!adjacent->second) {
		    complete = false;
		    break;
		}
		const ON_BrepTrim &original = brep->m_T[adjacent->first];
		const ON_BrepEdge *adjacent_edge = original.Edge();
		ON_NurbsCurve adjacent_edge_nurbs;
		if (!adjacent_edge || !adjacent_edge->GetNurbForm(adjacent_edge_nurbs)) {
		    complete = false;
		    break;
		}
		double tolerance = std::max(LocalUnits::tolerance,
		    adjacent_edge->m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(original.m_tolerance[0], original.m_tolerance[1]));
		for (int endpoint = 0; endpoint < 2; ++endpoint)
		    if (original.m_vi[endpoint] >= 0 &&
			    original.m_vi[endpoint] < brep->m_V.Count())
			tolerance = std::max(tolerance,
			    brep->m_V[original.m_vi[endpoint]].m_tolerance);
		const ON_Interval trim_domain = original.Domain();
		const ON_Interval edge_domain = adjacent_edge_nurbs.Domain();
		const int samples = std::min(4096,
		    std::max(64, std::max(original.SpanCount(),
			adjacent->second->SpanCount()) * 8));
		for (int sample = 0; complete && sample <= samples; ++sample) {
		    const double fraction = static_cast<double>(sample) / samples;
		    const double parameter = trim_domain.ParameterAt(fraction);
		    const ON_3dPoint original_uv = original.PointAt(parameter);
		    const ON_3dPoint candidate_uv = adjacent->second->PointAt(parameter);
		    const ON_3dPoint original_lift = surface->PointAt(
			original_uv.x, original_uv.y);
		    const ON_3dPoint candidate_lift = surface->PointAt(
			candidate_uv.x, candidate_uv.y);
		    if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			    original_lift.DistanceTo(candidate_lift) > tolerance) {
			complete = false;
			break;
		    }
		    const ON_3dPoint edge_point = adjacent_edge_nurbs.PointAt(
			edge_domain.ParameterAt(original.m_bRev3d ?
			    1.0 - fraction : fraction));
		    double edge_distance = edge_point.IsValid() ?
			candidate_lift.DistanceTo(edge_point) : DBL_MAX;
		    if (edge_distance > tolerance) {
			double edge_parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&edge_parameter,
				&adjacent_edge_nurbs, candidate_lift))
			    edge_distance = std::min(edge_distance,
				candidate_lift.DistanceTo(adjacent_edge_nurbs.PointAt(
				    edge_parameter)));
		    }
		    complete = edge_distance <= tolerance;
		}
	    }
	    const bool complementary = candidates.size() == 2 &&
		((candidates[0].iso == ON_Surface::W_iso &&
		  candidates[1].iso == ON_Surface::E_iso) ||
		 (candidates[0].iso == ON_Surface::E_iso &&
		  candidates[1].iso == ON_Surface::W_iso) ||
		 (candidates[0].iso == ON_Surface::S_iso &&
		  candidates[1].iso == ON_Surface::N_iso) ||
		 (candidates[0].iso == ON_Surface::N_iso &&
		  candidates[1].iso == ON_Surface::S_iso));
	    needs_repair = needs_repair || (complementary &&
		(candidates[0].iso != brep->m_T[candidates[0].trim_index].m_iso ||
		 candidates[1].iso != brep->m_T[candidates[1].trim_index].m_iso));
	    if (!complete || !needs_repair || !complementary) {
		if (wrapper->Verbose() && (!complete || needs_repair))
		    std::cerr << entity_type << " #" << entity_id << ": paired seam "
			<< pair->first << " in loop " << li
			<< " endpoint reconciliation rejected: complete="
			<< (complete ? "yes" : "no") << ", candidates="
			<< candidates.size() << ", adjacent="
			<< adjacent_candidates.size() << ", complementary="
			<< (complementary ? "yes" : "no")
			<< (rejection_reason.empty() ? "" :
			    ", reason=" + rejection_reason) << std::endl;
		continue;
	    }

	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    bool installed = true;
	    for (std::map<int, std::unique_ptr<ON_Curve> >::iterator adjacent =
		    adjacent_candidates.begin(); installed &&
		    adjacent != adjacent_candidates.end(); ++adjacent) {
		ON_Curve *curve = adjacent->second.release();
		const int c2 = brep->AddTrimCurve(curve);
		if (c2 < 0 || !brep->SetTrimCurve(brep->m_T[adjacent->first], c2)) {
		    if (c2 < 0)
			delete curve;
		    installed = false;
		    break;
		}
		brep->SetTrimIsoFlags(brep->m_T[adjacent->first]);
	    }
	    for (std::vector<Candidate>::iterator candidate = candidates.begin();
		    installed && candidate != candidates.end(); ++candidate) {
		const int c2 = brep->AddTrimCurve(candidate->curve.release());
		if (c2 < 0 || candidate->trim_index < 0 ||
			candidate->trim_index >= brep->m_T.Count() ||
			!brep->SetTrimCurve(brep->m_T[candidate->trim_index], c2)) {
		    installed = false;
		    break;
		}
		brep->m_T[candidate->trim_index].m_type = ON_BrepTrim::seam;
		brep->m_T[candidate->trim_index].m_iso = candidate->iso;
	    }
	    for (int lti = 0; installed && lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *current = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		installed = current && next && current->PointAtEnd().DistanceTo(
		    next->PointAtStart()) <= ON_ZERO_TOLERANCE;
	    }
	    if (!installed) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": paired seam "
			<< pair->first << " in loop " << li
			<< " endpoint reconciliation rolled back after installation"
			<< std::endl;
		*brep = *rollback;
		continue;
	    }
	    ++repaired_pairs;
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched a paired periodic seam to all exact adjacent boundary endpoints");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": paired seam "
		    << pair->first << " in loop " << li
		    << " matched to complementary adjacent boundaries" << std::endl;
	}
    }
    return repaired_pairs;
}


void
repair_zero_length_boundary_edges(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    /* A BREP bounding box may include the arbitrary finite proxy domain of an
     * unbounded analytic surface (a plane is the important case).  Such a
     * proxy must not enlarge a geometry-inference allowance.  Bound the item
     * by its authoritative topology vertices instead; the neighboring-edge
     * limit below independently constrains the affected local feature. */
    ON_BoundingBox item_bounds = ON_BoundingBox::EmptyBoundingBox;
    for (int vi = 0; vi < brep->m_V.Count(); ++vi)
	if (brep->m_V[vi].point.IsValid())
	    item_bounds.Set(brep->m_V[vi].point, true);
    const double item_scale = item_bounds.IsValid() ?
	item_bounds.Diagonal().Length() : 0.0;
    const double item_spur_limit = std::max(LocalUnits::tolerance,
	item_scale * kZeroLengthTopologyMaximumRelativeItemMismatch);
    const int repair_budget = brep->m_E.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	    ON_BrepTrim &trim = brep->m_T[ti];
	    if (trim.m_type != ON_BrepTrim::boundary || trim.m_ei < 0 ||
		    trim.m_ei >= brep->m_E.Count() || trim.m_li < 0 ||
		    trim.m_li >= brep->m_L.Count())
		continue;
	    ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	    ON_BrepLoop &loop = brep->m_L[trim.m_li];
	    if (edge.m_ti.Count() != 1 || edge.m_vi[0] < 0 ||
		    edge.m_vi[0] >= brep->m_V.Count() || edge.m_vi[1] < 0 ||
		    edge.m_vi[1] >= brep->m_V.Count() || loop.TrimCount() < 2)
		continue;
	    int trim_offset = -1;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		if (loop.m_ti[lti] == ti) {
		    trim_offset = lti;
		    break;
		}
	    }
	    if (trim_offset < 0)
		continue;
	    ON_BrepTrim *previous = loop.Trim(
		(trim_offset + loop.TrimCount() - 1) % loop.TrimCount());
	    ON_BrepTrim *next = loop.Trim((trim_offset + 1) % loop.TrimCount());
	    if (!previous || !next)
		continue;
	    const ON_Curve *curve = edge.EdgeCurveOf();
	    if (!curve)
		continue;
	    const ON_3dPoint vertex = brep->m_V[edge.m_vi[0]].point;
	    /* A finite sample can miss a narrow excursion of a high-degree spline.
	     * Bound the complete proxy-curve locus conservatively with its model-
	     * space bounding box instead: every curve point lies in the box, and
	     * the farthest box corner therefore bounds its distance from the
	     * asserted zero-length vertex. */
	    const ON_BoundingBox edge_bounds = edge.BoundingBox();
	    double maximum_radius = DBL_MAX;
	    if (edge_bounds.IsValid() && vertex.IsValid()) {
		maximum_radius = 0.0;
		for (int x = 0; x < 2; ++x) {
		    for (int y = 0; y < 2; ++y) {
			for (int z = 0; z < 2; ++z) {
			    const ON_3dPoint corner(
				x ? edge_bounds.m_max.x : edge_bounds.m_min.x,
				y ? edge_bounds.m_max.y : edge_bounds.m_min.y,
				z ? edge_bounds.m_max.z : edge_bounds.m_min.z);
			    maximum_radius = std::max(maximum_radius,
				corner.DistanceTo(vertex));
			}
		    }
		}
	    }
	    /* A source-asserted zero-length topology edge can carry a short open
	     * spline spur whose endpoint and surface discrepancies have already
	     * established a larger, densely measured local edge tolerance.  Once
	     * the complete spur is proven below both adjacent-feature and item-scale
	     * bounds, removing its sole boundary use follows the authoritative
	     * same-vertex topology.  Never apply this measured allowance in --exact
	     * mode or to a genuinely closed small loop, where it could erase real
	     * geometry. */
	    const double measured_local_tolerance = std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
	    const auto adjacent_edge_scale = [](const ON_BrepTrim *adjacent) {
		const ON_BrepEdge *adjacent_edge = adjacent ? adjacent->Edge() : NULL;
		if (!adjacent_edge)
		    return 0.0;
		const ON_BoundingBox bounds = adjacent_edge->BoundingBox();
		return bounds.IsValid() ? bounds.Diagonal().Length() : 0.0;
	    };
	    const double previous_scale = adjacent_edge_scale(previous);
	    const double next_scale = adjacent_edge_scale(next);
	    double neighboring_scale = 0.0;
	    if (previous_scale > 0.0 && next_scale > 0.0)
		neighboring_scale = std::min(previous_scale, next_scale);
	    else
		neighboring_scale = std::max(previous_scale, next_scale);
	    const double neighborhood_spur_limit = neighboring_scale > 0.0 ?
		neighboring_scale *
		    kZeroLengthTopologyMaximumRelativeNeighborMismatch : 0.0;
	    const double geometric_endpoint_gap =
		edge.PointAtStart().DistanceTo(edge.PointAtEnd());
	    const double literal_closure_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		LocalUnits::tolerance * 1.0e-8);
	    const bool measured_open_spur = !wrapper->ImportOptions().exact &&
		edge.m_vi[0] == edge.m_vi[1] &&
		geometric_endpoint_gap > literal_closure_tolerance &&
		item_scale > 0.0 && neighboring_scale > 0.0 &&
		measured_local_tolerance > LocalUnits::tolerance;
	    const double zero_length_tolerance = measured_open_spur ?
		std::min(item_spur_limit, neighborhood_spur_limit) :
		LocalUnits::tolerance;
	    const bool zero_length = std::isfinite(maximum_radius) &&
		maximum_radius <= zero_length_tolerance;
	    double exposed_gap = previous->PointAtEnd().DistanceTo(
		next->PointAtStart());
	    if (wrapper->Verbose() && (edge.m_vi[0] == edge.m_vi[1] ||
		    brep->m_V[edge.m_vi[0]].point.DistanceTo(
			brep->m_V[edge.m_vi[1]].point) <= LocalUnits::tolerance))
		std::cerr << entity_type << " #" << entity_id
		    << ": zero-length boundary candidate trim " << ti
		    << "/STEP edge " << edge.m_edge_user.i << " vertices "
		    << edge.m_vi[0] << '/' << edge.m_vi[1] << " surrounding "
		    << previous->m_vi[1] << '/' << next->m_vi[0]
		    << " vertex-STEP "
		    << brep->m_V[edge.m_vi[0]].m_vertex_user.i << '/'
		    << brep->m_V[edge.m_vi[1]].m_vertex_user.i
		    << " vertex-gap "
		    << brep->m_V[edge.m_vi[0]].point.DistanceTo(
			brep->m_V[edge.m_vi[1]].point)
		    << " p-gap " << exposed_gap << " max-radius " << maximum_radius
		    << " tolerance " << zero_length_tolerance
		    << std::endl;
	    /* A native-seam split can create two independently indexed vertices
	     * for one source point.  If the complete one-use child edge lies
	     * inside the asserted model uncertainty and at least one endpoint is
	     * importer-created, collapse and remove that child transactionally.
	     * This is the distinct-vertex form of the zero-length repair below;
	     * authoritative STEP vertex pairs remain untouched. */
	    if (edge.m_vi[0] != edge.m_vi[1] &&
		    brlcad::step::ImporterSplitEdgeIsToleranceDegenerate(
			edge, LocalUnits::tolerance) &&
		    previous->m_vi[1] == trim.m_vi[0] &&
		    next->m_vi[0] == trim.m_vi[1]) {
		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		const int affected_face = trim.Face() ?
		    trim.Face()->m_face_index : -1;
		const auto face_join_gaps = [](const ON_Brep &source,
			int face_index, size_t &count, double &maximum) {
		    count = 0;
		    maximum = 0.0;
		    if (face_index < 0 || face_index >= source.m_F.Count())
			return false;
		    const ON_BrepFace &source_face = source.m_F[face_index];
		    for (int fli = 0; fli < source_face.m_li.Count(); ++fli) {
			const int source_li = source_face.m_li[fli];
			if (source_li < 0 || source_li >= source.m_L.Count())
			    return false;
			const ON_BrepLoop &source_loop = source.m_L[source_li];
			for (int lti = 0; lti < source_loop.TrimCount(); ++lti) {
			    const ON_BrepTrim *current = source_loop.Trim(lti);
			    const ON_BrepTrim *following = source_loop.Trim(
				(lti + 1) % source_loop.TrimCount());
			    if (!current || !following)
				return false;
			    const ON_3dPoint end = current->PointAtEnd();
			    const ON_3dPoint start = following->PointAtStart();
			    const double gap = end.IsValid() && start.IsValid() ?
				end.DistanceTo(start) : DBL_MAX;
			    if (gap > ON_ZERO_TOLERANCE) {
				++count;
				maximum = std::max(maximum, gap);
			    }
			}
		    }
		    return true;
		};
		ON_wString before_messages;
		ON_TextLog before_log(before_messages);
		const bool before_valid = brep->IsValid(&before_log);
		ON_wString before_face_messages;
		ON_TextLog before_face_log(before_face_messages);
		const bool before_face_valid = affected_face >= 0 &&
		    affected_face < brep->m_F.Count() &&
		    brep->m_F[affected_face].IsValid(&before_face_log);
		size_t before_gap_count = 0;
		double before_maximum_gap = 0.0;
		const bool before_gaps_audited = face_join_gaps(*brep,
		    affected_face, before_gap_count, before_maximum_gap);
		const int first_vertex = candidate->m_T[ti].m_vi[0];
		const int second_vertex = candidate->m_T[ti].m_vi[1];
		std::string collapse_failure;
		bool installed = first_vertex >= 0 && second_vertex >= 0 &&
		    first_vertex < candidate->m_V.Count() &&
		    second_vertex < candidate->m_V.Count() &&
		    candidate->CombineCoincidentVertices(
			candidate->m_V[first_vertex],
			candidate->m_V[second_vertex]);
		if (!installed)
		    collapse_failure = "could not combine the coincident vertices";
		ON_BrepTrim *candidate_previous = installed ?
		    candidate->Trim(previous->m_trim_index) : NULL;
		ON_BrepTrim *candidate_next = installed ?
		    candidate->Trim(next->m_trim_index) : NULL;
		if (installed && candidate_previous && candidate_next &&
			candidate_previous->PointAtEnd().DistanceTo(
			    candidate_next->PointAtStart()) > ON_ZERO_TOLERANCE)
		    installed = candidate->MatchTrimEnds(
			*candidate_previous, *candidate_next);
		if (!installed && collapse_failure.empty())
		    collapse_failure = "could not reconcile the exposed pcurve join";
		if (installed && ti >= 0 && ti < candidate->m_T.Count())
		    candidate->DeleteTrim(candidate->m_T[ti], true);
		else
		    installed = false;
		if (!installed && collapse_failure.empty())
		    collapse_failure = "could not delete the degenerate child trim";
		ON_wString validation_messages;
		ON_TextLog validation_log(validation_messages);
		std::string unsafe_topology;
		installed = installed && candidate->Compact() &&
		    candidate->SetTrimTypeFlags(false) &&
		    brep_topology_references_are_safe(candidate.get(),
			&unsafe_topology);
		const bool after_valid =
		    installed && candidate->IsValid(&validation_log);
		ON_wString after_face_messages;
		ON_TextLog after_face_log(after_face_messages);
		const bool after_face_valid = installed && affected_face >= 0 &&
		    affected_face < candidate->m_F.Count() &&
		    candidate->m_F[affected_face].IsValid(&after_face_log);
		size_t after_gap_count = 0;
		double after_maximum_gap = 0.0;
		const bool after_gaps_audited = installed && face_join_gaps(
		    *candidate, affected_face, after_gap_count,
		    after_maximum_gap);
		const bool join_regression = !before_gaps_audited ||
		    !after_gaps_audited ||
		    after_gap_count > before_gap_count ||
		    after_maximum_gap > std::max(
			100.0 * ON_ZERO_TOLERANCE,
			100.0 * before_maximum_gap);
		if (installed && ((before_valid && !after_valid) ||
			(before_face_valid && !after_face_valid) ||
			join_regression)) {
		    installed = false;
		    collapse_failure =
			"candidate worsened structural or face-join validation";
		}
		if (installed) {
		    const int source_edge = edge.m_edge_user.i;
		    *brep = *candidate;
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"removed an importer-created zero-length split edge "
			"within model tolerance");
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": collapsed tolerance-degenerate child of STEP "
			    << "edge " << source_edge << " between anonymous "
			    << "split vertices" << std::endl;
		    changed = true;
		    break;
		}
		if (wrapper->Verbose()) {
		    ON_String text(validation_messages);
		    std::cerr << entity_type << " #" << entity_id
			<< ": tolerance-degenerate STEP edge "
			<< edge.m_edge_user.i << "/T" << ti
			<< " collapse rejected: "
			<< (!collapse_failure.empty() ? collapse_failure :
			    (!unsafe_topology.empty() ? unsafe_topology :
				(text.Array() && text.Array()[0] ? text.Array() :
				    "complete BREP validation failed")))
			<< std::endl;
		}
	    }
	    if (edge.m_vi[0] != edge.m_vi[1] ||
		    previous->m_vi[1] != next->m_vi[0])
		continue;
	    if (!zero_length)
		continue;
	    if (exposed_gap > ON_ZERO_TOLERANCE) {
		const ON_BrepFace *face = loop.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!surface)
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint common_uv = 0.5 * (previous_uv + next_uv);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		const ON_3dPoint common_lift = surface->PointAt(common_uv.x, common_uv.y);
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			!common_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > zero_length_tolerance ||
			next_lift.DistanceTo(vertex) > zero_length_tolerance ||
			common_lift.DistanceTo(vertex) > zero_length_tolerance ||
			previous_lift.DistanceTo(next_lift) >
			    zero_length_tolerance)
		    continue;
		if (!brep->MatchTrimEnds(*previous, *next))
		    continue;
		exposed_gap = previous->PointAtEnd().DistanceTo(next->PointAtStart());
		if (exposed_gap > ON_ZERO_TOLERANCE)
		    continue;
	    }
	    const int source_edge = edge.m_edge_user.i;
	    brep->DeleteTrim(trim, true);
	    if (!brep->Compact())
		return;
	    if (measured_open_spur)
	    {
		std::ostringstream warning;
		warning << "open spline geometry contradicted a zero-length STEP "
		    "topology edge; removed its single boundary use after the "
		    "complete locus radius " << maximum_radius
		    << " mm fit the bounded neighboring-feature/item limit "
		    << zero_length_tolerance << " mm";
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, source_edge,
		    "EDGE_CURVE", "edge_geometry", warning.str());
		if (wrapper->CurveInferenceTrialEnabled()) {
		    std::ostringstream detail;
		    detail << "removed the sole boundary use after its complete "
			"locus radius " << maximum_radius
			<< " mm was below both the neighboring-feature limit "
			<< neighborhood_spur_limit << " mm and item-scale limit "
			<< item_spur_limit << " mm";
		    wrapper->RecordInferredCurve(source_edge,
			"zero_length_topology_spur_removal", maximum_radius,
			LocalUnits::tolerance, zero_length_tolerance,
			LocalUnits::tolerance, detail.str());
		}
	    }
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		measured_open_spur ?
		"removed an open spline spur from a zero-length STEP topology edge within its measured local tolerance" :
		"removed a zero-length boundary edge within model tolerance");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": removed zero-length boundary STEP edge " << source_edge
		    << " within tolerance " << zero_length_tolerance << std::endl;
	    changed = true;
	    break;
	}
	if (!changed)
	    break;
    }
}


bool
trim_orientation_toggle_preserves_edge_pair(const ON_Brep *brep,
	const ON_BrepTrim &trim)
{
    if (!brep || trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count())
	return false;
    const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
    if (edge.m_ti.Count() != 2)
	return true;

    const ON_BrepTrim *other = NULL;
    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	const int ti = edge.m_ti[eti];
	if (ti < 0 || ti >= brep->m_T.Count())
	    return false;
	if (ti != trim.m_trim_index)
	    other = &brep->m_T[ti];
    }
    const ON_BrepFace *face = trim.Face();
    const ON_BrepFace *other_face = other ? other->Face() : NULL;
    if (!other || !face || !other_face)
	return false;

    /* OpenNURBS requires the two uses of every mated or seam edge to have
     * opposite effective directions.  A closed pcurve has no distinct topology
     * vertices to expose an accidental flag reversal, so a tangent-only repair
     * used to be able to corrupt an already consistent STEP edge pair.  Permit
     * a flag change only when the completed edge topology proves that it keeps
     * (or restores) the required opposite-use invariant. */
    const bool toggled_effective = (!trim.m_bRev3d) ^ face->m_bRev;
    const bool other_effective = other->m_bRev3d ^ other_face->m_bRev;
    return toggled_effective != other_effective;
}


void
repair_final_closed_trim_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper)
	return;

    /* The final adjacent-endpoint pass can replace the endpoint control point
     * of a closed pcurve after the main orientation pass.  Verify the exact
     * endpoint tangents again at that point.  A flag reversal is allowed only
     * when both endpoint tangents strongly prove the same correction;
     * ambiguous or mixed directions are left for structural validation. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		trim.m_vi[0] != trim.m_vi[1])
	    continue;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	if (edge.m_vi[0] != edge.m_vi[1])
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	if (!surface)
	    continue;
	/* A prior dense source edge/surface measurement may have established a
	 * larger local OpenNURBS tolerance in non-exact mode.  Orientation checks
	 * must use that verified value too; otherwise the same trim is accepted for
	 * locus reconstruction and then rejected when proving its direction. */
	const double orientation_tolerance = std::max(LocalUnits::tolerance,
	    std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1])));

	double alignment[2];
	closed_trim_endpoint_alignments(trim, edge, surface, alignment);
	const bool alignment_valid[2] = {
	    closed_trim_endpoint_alignment_is_valid(alignment[0]),
	    closed_trim_endpoint_alignment_is_valid(alignment[1])
	};
	const bool negative_alignment[2] = {
	    alignment_valid[0] && alignment[0] < 0.0,
	    alignment_valid[1] && alignment[1] < 0.0
	};
	if (!negative_alignment[0] && !negative_alignment[1])
	    continue;
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": final closed trim "
		<< ti << " endpoint dots=" << alignment[0] << ',' << alignment[1]
		<< std::endl;

	if (alignment_valid[0] && alignment_valid[1] &&
		alignment[0] <= -0.9 && alignment[1] <= -0.9 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected a closed-edge trim orientation after endpoint repair");
	    continue;
	}

	/* A periodic closed curve can have an unreliable one-sided derivative at
	 * its parameter seam.  The main orientation pass already protects a trim
	 * whose directed interior correspondence is uniformly correct; retain that
	 * protection here as well.  Otherwise this final endpoint-only pass can
	 * regenerate one physical boundary of a newly constructed periodic band
	 * onto the opposite domain image and reopen both seam joins. */
	int matching_samples = 0;
	int opposing_samples = 0;
	ON_NurbsCurve edge_nurbs;
	if (edge.GetNurbForm(edge_nurbs)) {
	    const ON_Interval trim_domain = trim.Domain();
	    const ON_Interval edge_domain = edge_nurbs.Domain();
	    for (int sample = 1; sample < 16; ++sample) {
		const double fraction = static_cast<double>(sample) / 16.0;
		ON_3dPoint uv;
		ON_3dVector uv_tangent;
		if (!trim.Ev1Der(trim_domain.ParameterAt(fraction), uv,
			uv_tangent))
		    continue;
		ON_3dPoint lifted_point;
		ON_3dVector du, dv;
		if (!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
		    continue;
		ON_3dVector lifted_tangent = uv_tangent.x * du +
		    uv_tangent.y * dv;
		const double edge_parameter = edge_domain.ParameterAt(
		    trim.m_bRev3d ? 1.0 - fraction : fraction);
		if (lifted_point.DistanceTo(edge_nurbs.PointAt(edge_parameter)) >
			orientation_tolerance)
		    continue;
		ON_3dVector edge_tangent = edge_nurbs.TangentAt(edge_parameter);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize())
		    continue;
		double dot = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d) dot = -dot;
		if (dot > 0.5)
		    ++matching_samples;
		else if (dot < -0.5)
		    ++opposing_samples;
	    }
	}
	if (matching_samples >= 5 && opposing_samples == 0)
	    continue;

	const ON_3dPoint *required_start = NULL;
	const ON_3dPoint *required_end = NULL;
	ON_3dPoint adjacent_start = ON_3dPoint::UnsetPoint;
	ON_3dPoint adjacent_end = ON_3dPoint::UnsetPoint;
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    if (loop.m_ti[lti] != ti)
		continue;
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (previous && previous->m_vi[1] == trim.m_vi[0]) {
		adjacent_start = previous->PointAtEnd();
		required_start = &adjacent_start;
	    }
	    if (next && trim.m_vi[1] == next->m_vi[0]) {
		adjacent_end = next->PointAtStart();
		required_end = &adjacent_end;
	    }
	    break;
	}

	std::string regeneration_failure;
	if (edge.GetNurbForm(edge_nurbs) &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    orientation_tolerance, &regeneration_failure, NULL,
		    required_start, required_end, true, wrapper)) {
	    closed_trim_endpoint_alignments(trim, edge, surface, alignment);
	    const bool regenerated_alignment_valid[2] = {
		closed_trim_endpoint_alignment_is_valid(alignment[0]),
		closed_trim_endpoint_alignment_is_valid(alignment[1])
	    };
		if (regenerated_alignment_valid[0] &&
			regenerated_alignment_valid[1] &&
			alignment[0] <= -0.9 && alignment[1] <= -0.9 &&
			trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
		trim.m_bRev3d = !trim.m_bRev3d;
		trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
		trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
		wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		    "corrected a regenerated closed-edge trim orientation");
	    }
	    if ((!regenerated_alignment_valid[0] || alignment[0] >= 0.0) &&
		    (!regenerated_alignment_valid[1] || alignment[1] >= 0.0))
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "regenerated a closed-edge pcurve after endpoint repair");
	} else if (wrapper->Verbose() && !regeneration_failure.empty()) {
	    std::cerr << entity_type << " #" << entity_id << ": final closed trim "
		<< ti << " pcurve regeneration rejected: "
		<< regeneration_failure << std::endl;
	}
    }
}


void
interpret_closed_periodic_face_bounds(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper)
	return;

    /* RP8 H.1.7: FACE_OUTER_BOUND is ambiguous on a closed periodic
     * cylinder, sphere, or torus and must not be treated as authoritative.
     * Interpret final loop roles from completed pcurves and override missing
     * or conflicting source designations when that interpretation identifies
     * exactly one outer loop.  This also runs in --exact mode: it is an AP203
     * reading rule, not a geometry repair. */
    bool inspected_any = false;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface || (!surface->IsClosed(0) && !surface->IsClosed(1))) continue;
	std::vector<ON_BrepLoop::TYPE> inferred_types;
	inferred_types.reserve(face.m_li.Count());
	int supplied_outer_count = 0;
	int inferred_outer_count = 0;
	bool inference_complete = face.m_li.Count() > 0;
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count()) {
		inference_complete = false;
		inferred_types.push_back(ON_BrepLoop::unknown);
		continue;
	    }
	    if (brep->m_L[li].m_type == ON_BrepLoop::outer)
		++supplied_outer_count;
	    const ON_BrepLoop::TYPE inferred = face.m_li.Count() == 1 ?
		ON_BrepLoop::outer : brep->ComputeLoopType(brep->m_L[li]);
	    inferred_types.push_back(inferred);
	    if (inferred == ON_BrepLoop::outer)
		++inferred_outer_count;
	    else if (inferred != ON_BrepLoop::inner &&
		    inferred != ON_BrepLoop::slit)
		inference_complete = false;
	}
	if (!inference_complete) continue;
	inspected_any = true;
	/* An already valid single-outer OpenNURBS classification is retained:
	 * periodic seam loops can be ambiguous to ComputeLoopType even after their
	 * exact pcurves are complete.  A missing or multiply supplied outer flag is
	 * replaced only when the pcurves identify exactly one outer loop. */
	if (supplied_outer_count != 1 && inferred_outer_count == 1) {
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		brep->m_L[li].m_type = inferred_types[fli];
	    }
	    brep->SortFaceLoops(face);
	}
    }

    if (inspected_any)
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    entity_id, entity_type, "face_bound",
	    "ignored FACE_OUTER_BOUND designations on closed periodic surfaces and interpreted final loop roles from exact pcurves");
}


void
repair_face_bound_classification(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;

    /* FACE_BOUND is the base type and does not itself say inner or outer.
     * Conforming files use FACE_OUTER_BOUND for exactly one member, but some
     * production exporters emit either only FACE_BOUND or more than one
     * FACE_OUTER_BOUND.  Recover or correct the classification only when it
     * is unambiguous: a face has one loop, or openNURBS computes exactly one
     * outer loop from the completed pcurves.  No curve or topology is changed. */
    if (wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    ON_BrepFace &face = brep->m_F[fi];
	    if (face_source_tags && face_source_tags->find(
		    face.m_face_user.i) == face_source_tags->end())
		continue;
	    int outer_count = 0;
	    bool classification_complete = true;
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) {
		    classification_complete = false;
		    continue;
		}
		const ON_BrepLoop::TYPE type = brep->m_L[li].m_type;
		if (type == ON_BrepLoop::outer)
		    ++outer_count;
		else if (type != ON_BrepLoop::inner && type != ON_BrepLoop::slit)
		    classification_complete = false;
	    }
	    if (face.m_li.Count() == 0)
		continue;
	    if (outer_count == 1 && classification_complete) {
		const int first_li = face.m_li[0];
		if (first_li >= 0 && first_li < brep->m_L.Count() &&
			brep->m_L[first_li].m_type == ON_BrepLoop::outer)
		    continue;
		brep->SortFaceLoops(face);
		wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		    "restored the outer FACE_BOUND loop to the first face-loop position");
		continue;
	    }

	    if (outer_count == 0 && face.m_li.Count() == 1) {
		const int li = face.m_li[0];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		brep->m_L[li].m_type = ON_BrepLoop::outer;
		wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		    "classified the only FACE_BOUND loop as the outer boundary");
		continue;
	    }

	    std::vector<ON_BrepLoop::TYPE> computed;
	    computed.reserve(face.m_li.Count());
	    int computed_outer_count = 0;
	    int computed_inner_count = 0;
	    bool computed_classification_complete = true;
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		const ON_BrepLoop::TYPE type = li >= 0 && li < brep->m_L.Count() ?
		    brep->ComputeLoopType(brep->m_L[li]) : ON_BrepLoop::unknown;
		computed.push_back(type);
		if (type == ON_BrepLoop::outer) ++computed_outer_count;
		else if (type == ON_BrepLoop::inner) ++computed_inner_count;
		else computed_classification_complete = false;
	    }
	    /* Some exporters encode every bound as FACE_BOUND and orient the
	     * complete p-space face oppositely to OpenNURBS convention.  The
	     * resulting signature is unambiguous: no declared outer, exactly one
	     * computed inner loop, and every other loop computed outer.  Complement
	     * all classifications together so the unique enclosing boundary becomes
	     * outer and the remaining loops become holes.  Do not apply this rule
	     * when any loop is unknown/slit or when more than one complementary
	     * outer candidate exists. */
	    if (outer_count == 0 && computed_classification_complete &&
		    computed_inner_count == 1 && computed_outer_count ==
			face.m_li.Count() - 1) {
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= brep->m_L.Count())
			continue;
		    brep->m_L[li].m_type = computed[fli] == ON_BrepLoop::inner ?
			ON_BrepLoop::outer : ON_BrepLoop::inner;
		}
		brep->SortFaceLoops(face);
		wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		    "classified a uniformly reversed FACE_BOUND loop set from its exact pcurves");
		continue;
	    }
	    if (computed_outer_count != 1)
	    {
		if (wrapper->Verbose() && face.m_li.Count() > 1) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": unresolved FACE_BOUND classification F" << fi
			<< " outer-count=" << outer_count
			<< " computed-outer-count=" << computed_outer_count;
		    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
			const int li = face.m_li[fli];
			if (li < 0 || li >= brep->m_L.Count()) continue;
			const ON_BrepLoop &loop = brep->m_L[li];
			std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			    << "=" << static_cast<int>(loop.m_type) << '/'
			    << static_cast<int>(computed[fli]) << "(trims="
			    << loop.TrimCount() << ')';
		    }
		    std::cerr << std::endl;
		}
		continue;
	    }
	    if (wrapper->Verbose() && face.m_li.Count() > 1) {
		std::cerr << entity_type << " #" << entity_id
		    << ": resolved FACE_BOUND classification F" << fi;
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= brep->m_L.Count()) continue;
		    const ON_BrepLoop &loop = brep->m_L[li];
		    std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			<< '=' << static_cast<int>(loop.m_type) << '/'
			<< static_cast<int>(computed[fli]) << "(trims="
			<< loop.TrimCount() << ')';
		}
		std::cerr << std::endl;
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li >= 0 && li < brep->m_L.Count() &&
			(computed[fli] == ON_BrepLoop::outer ||
			 computed[fli] == ON_BrepLoop::inner ||
			 computed[fli] == ON_BrepLoop::slit))
		    brep->m_L[li].m_type = computed[fli];
	    }
	    brep->SortFaceLoops(face);
	    wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		outer_count == 0 ?
		"classified untyped FACE_BOUND loops from their exact pcurves" :
		outer_count > 1 ?
		"corrected multiple FACE_OUTER_BOUND loops from their exact pcurves" :
		"completed FACE_BOUND loop classification from exact pcurves");
	}
    }
}


/* Invalid exporters occasionally put FACE_OUTER_BOUND on several members of
 * one face.  If exact pcurve winding cannot classify those members directly,
 * safe mode may test each supplied alternative while preserving every loop
 * and edge.  A candidate changes only its owning face's loop classification;
 * solidness depends on the unchanged edge-use topology.  Validate alternatives
 * on that face and its edge-adjacent faces, then validate the one assembled
 * whole-solid result.  This makes the proof linear instead of enumerating the
 * Cartesian product of independent face choices.  Keep the aggregate local
 * proof bounded at 1024 candidates: that covers hundreds of ordinary binary
 * exporter mistakes while preventing malformed input from forcing an
 * unbounded number of exact OpenNURBS validations. */
constexpr size_t kMaximumFaceBoundClassificationCandidates = 1024;


std::vector<SuppliedFaceOuterBounds>
supplied_face_outer_bound_ambiguities(const ON_Brep *brep)
{
    std::vector<SuppliedFaceOuterBounds> ambiguities;
    if (!brep)
	return ambiguities;

    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	SuppliedFaceOuterBounds supplied;
	supplied.face_index = fi;
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		continue;
	    const ON_BrepLoop &loop = brep->m_L[li];
	    if (loop.m_type != ON_BrepLoop::outer || loop.m_loop_user.i <= 0)
		continue;
	    SuppliedOuterBoundCandidate candidate;
	    candidate.loop_step_id = loop.m_loop_user.i;
	    std::map<int, size_t> edge_uses;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (edge && edge->m_edge_user.i > 0)
		    ++edge_uses[edge->m_edge_user.i];
	    }
	    /* An edge used once in a bound is its persistent boundary geometry.
	     * Opposite paired uses are zero-area keyhole bridges and may be removed
	     * during exact periodic-band normalization, so they are not provenance
	     * evidence for the supplied loop after that repair. */
	    for (std::map<int, size_t>::const_iterator edge = edge_uses.begin();
		    edge != edge_uses.end(); ++edge)
		if (edge->second == 1)
		    candidate.nonbridge_edge_step_ids.push_back(edge->first);
	    supplied.candidates.push_back(candidate);
	}
	if (supplied.candidates.size() > 1)
	    ambiguities.push_back(supplied);
    }
    return ambiguities;
}


bool
retry_unique_face_bound_classification(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, bool require_solid,
	const std::vector<SuppliedFaceOuterBounds> &supplied_ambiguities)
{
    /* A merely valid surface model cannot distinguish the intended side of
     * an ambiguous loop.  Only a closed-solid result supplies the additional
     * evidence needed to call one interpretation a repair. */
    if (!brep || !wrapper || !require_solid ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    wrapper->ImportOptions().exact)
	return false;
    if (wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id << ": testing "
	    << supplied_ambiguities.size()
	    << " supplied FACE.wr2 ambiguity set(s)" << std::endl;

    struct AmbiguousFace {
	int face_index;
	std::vector<int> candidate_loops;
    };
    std::vector<AmbiguousFace> ambiguous_faces;
    size_t candidate_count = 0;
    for (std::vector<SuppliedFaceOuterBounds>::const_iterator supplied =
	    supplied_ambiguities.begin(); supplied != supplied_ambiguities.end();
	    ++supplied) {
	AmbiguousFace ambiguous;
	ambiguous.face_index = -1;

	/* Resolve the owning face before using edge provenance to recover a
	 * merged loop.  A boundary edge is normally shared by an adjacent face,
	 * so searching the complete BREP for one source candidate at a time can
	 * make the first missing loop look ambiguous even though another
	 * candidate's retained STEP loop ID identifies the face uniquely.
	 * Compaction may change array indices, but it does not change surviving
	 * STEP loop IDs. */
	for (std::vector<SuppliedOuterBoundCandidate>::const_iterator source =
		supplied->candidates.begin();
		source != supplied->candidates.end(); ++source) {
	    int matching_loop = -1;
	    int matching_face = -1;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		if (loop.m_loop_user.i != source->loop_step_id)
		    continue;
		const ON_BrepFace *owner = loop.Face();
		if (!owner || owner->m_face_index < 0 ||
			owner->m_face_index >= brep->m_F.Count())
		    return false;
		if (matching_loop >= 0) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": supplied STEP loop #" << source->loop_step_id
			    << " was split; FACE_BOUND trial is unsafe" << std::endl;
		    return false;
		}
		matching_loop = li;
		matching_face = owner->m_face_index;
	    }
	    if (matching_loop < 0)
		continue;
	    if (ambiguous.face_index < 0)
		ambiguous.face_index = matching_face;
	    else if (matching_face != ambiguous.face_index) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": surviving supplied FACE_BOUND loop IDs from original F"
			<< supplied->face_index << " resolve to distinct faces F"
			<< ambiguous.face_index << " and F" << matching_face
			<< "; classification trial is unsafe" << std::endl;
		return false;
	    }
	}

	for (std::vector<SuppliedOuterBoundCandidate>::const_iterator source =
		supplied->candidates.begin();
		source != supplied->candidates.end(); ++source) {
	    int matching_loop = -1;
	    int matching_face = -1;
	    /* Repair may compact the BREP before this deferred retry.  The
	     * captured face array index is therefore diagnostic provenance, not
	     * stable identity.  Re-resolve the owning face from each supplied STEP
	     * loop ID (or its persistent non-bridge edge IDs) in the current BREP,
	     * and require all alternatives to still belong to one face. */
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		if (loop.m_loop_user.i != source->loop_step_id)
		    continue;
		const ON_BrepFace *owner = loop.Face();
		if (!owner || owner->m_face_index < 0 ||
			owner->m_face_index >= brep->m_F.Count())
		    return false;
		if (ambiguous.face_index >= 0 &&
			owner->m_face_index != ambiguous.face_index)
		    continue;
		/* A topology split can create several OpenNURBS loops from one
		 * supplied bound.  That is no longer a simple choice among the
		 * original inputs, so do not guess how the split pieces classify. */
		if (matching_loop >= 0) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": supplied STEP loop #" << source->loop_step_id
			    << " was split; FACE_BOUND trial is unsafe" << std::endl;
		    return false;
		}
		matching_loop = li;
		matching_face = owner->m_face_index;
	    }
	    /* Exact periodic-band repair combines two supplied boundary rings into
	     * one OpenNURBS loop and necessarily retains only one loop user label.
	     * Recover the other source identity from its non-bridge STEP edges.
	     * Once any surviving source loop has identified the owning face,
	     * restrict this search to that face: reciprocal boundary edges also
	     * occur on neighboring faces and are not ambiguous within the source
	     * FACE_BOUND alternatives. */
	    if (matching_loop < 0 && !source->nonbridge_edge_step_ids.empty()) {
		for (int li = 0; li < brep->m_L.Count(); ++li) {
		    const ON_BrepLoop &current = brep->m_L[li];
		    const ON_BrepFace *owner = current.Face();
		    if (!owner || owner->m_face_index < 0 ||
			    owner->m_face_index >= brep->m_F.Count())
			return false;
		    if (ambiguous.face_index >= 0 &&
			    owner->m_face_index != ambiguous.face_index)
			continue;
		    std::set<int> current_edges;
		    for (int lti = 0; lti < current.TrimCount(); ++lti) {
			const ON_BrepTrim *trim = current.Trim(lti);
			const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
			if (edge && edge->m_edge_user.i > 0)
			    current_edges.insert(edge->m_edge_user.i);
		    }
		    bool contains_source = true;
		    for (std::vector<int>::const_iterator edge =
			    source->nonbridge_edge_step_ids.begin();
			    edge != source->nonbridge_edge_step_ids.end(); ++edge)
			contains_source = contains_source &&
			    current_edges.find(*edge) != current_edges.end();
		    if (!contains_source)
			continue;
		    if (matching_loop >= 0) {
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": supplied STEP loop #" << source->loop_step_id
				<< " has ambiguous post-repair edge provenance; "
			    "FACE_BOUND trial is unsafe" << std::endl;
			return false;
		    }
		    matching_loop = li;
		    matching_face = owner->m_face_index;
		}
	    }
	    if (matching_loop < 0) {
		if (wrapper->Verbose())
		{
		    std::cerr << entity_type << " #" << entity_id
			<< ": supplied STEP loop #" << source->loop_step_id
			<< " from original face F" << supplied->face_index
			<< " no longer exists in the repaired BREP";
		    std::cerr << "; FACE_BOUND trial is unsafe" << std::endl;
		}
		return false;
	    }
	    if (ambiguous.face_index < 0)
		ambiguous.face_index = matching_face;
	    else if (matching_face != ambiguous.face_index) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": supplied FACE_BOUND candidates from original F"
			<< supplied->face_index << " now resolve to distinct faces F"
			<< ambiguous.face_index << " and F" << matching_face
			<< "; classification trial is unsafe" << std::endl;
		return false;
	    }
	    ambiguous.candidate_loops.push_back(matching_loop);
	}
	if (ambiguous.face_index < 0 ||
		ambiguous.face_index >= brep->m_F.Count())
	    return false;
	std::sort(ambiguous.candidate_loops.begin(),
	    ambiguous.candidate_loops.end());
	ambiguous.candidate_loops.erase(std::unique(
	    ambiguous.candidate_loops.begin(), ambiguous.candidate_loops.end()),
	    ambiguous.candidate_loops.end());
	if (ambiguous.candidate_loops.size() == 1 &&
		supplied->candidates.size() > 1) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact topology retained all "
		    << supplied->candidates.size()
		    << " FACE_OUTER_BOUND components in one periodic face loop L"
		    << ambiguous.candidate_loops.front() << std::endl;
	    continue;
	}
	if (ambiguous.candidate_loops.size() != supplied->candidates.size()) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": supplied FACE_BOUND alternatives were partially merged; "
		       "classification trial is unsafe" << std::endl;
	    return false;
	}
	/* This fallback answers only an actual FACE.wr2 ambiguity: which of the
	 * competing supplied outer bounds was intended to enclose the face?  Do
	 * not turn unrelated unknown/slit classifications into combinatorial
	 * guesses.  Existing exact-pcurve classification handles those cases.
	 * The source identities were captured before those derived classifications
	 * could replace the invalid exporter flags. */
	if (ambiguous.candidate_loops.size() <= 1)
	    continue;
	if (ambiguous.candidate_loops.size() >
		kMaximumFaceBoundClassificationCandidates - candidate_count) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		entity_id, entity_type, "face_bound",
		"ambiguous FACE_BOUND interpretations exceeded the bounded local-candidate validation limit");
	    return false;
	}
	candidate_count += ambiguous.candidate_loops.size();
	ambiguous_faces.push_back(ambiguous);
    }
    if (ambiguous_faces.empty() ||
	    candidate_count > kMaximumFaceBoundClassificationCandidates)
	return false;

    /* Outer/inner classification cannot repair an open or nonmanifold edge
     * graph, so require the unchanged STEP topology to be a closed manifold.
     * Do not require it to be oriented yet: face-orientation repair runs only
     * after OpenNURBS structural validation, and requiring IsSolid() here
     * creates a deadlock when one invalid FACE.wr2 label is the sole
     * structural error. */
    bool source_oriented = false;
    bool source_has_boundary = true;
    const bool source_manifold = brep->IsManifold(&source_oriented,
	&source_has_boundary);
    if (require_solid && (!source_manifold || source_has_boundary)) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": supplied FACE_BOUND alternatives cannot repair non-solid "
		   "edge-use topology (manifold="
		<< (source_manifold ? "yes" : "no") << ", boundary="
		<< (source_has_boundary ? "yes" : "no") << ')' << std::endl;
	return false;
    }
    if (require_solid && !source_oriented && wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id
	    << ": testing FACE_BOUND alternatives on closed manifold topology "
	       "before the separate face-orientation repair" << std::endl;

    std::vector<int> unique_outer_loops;
    unique_outer_loops.reserve(ambiguous_faces.size());
    size_t completed_candidates = 0;
    for (std::vector<AmbiguousFace>::const_iterator ambiguous =
	    ambiguous_faces.begin(); ambiguous != ambiguous_faces.end();
	    ++ambiguous) {
	if (brlcad::PullbackWorkCancelled())
	    return false;

	/* Duplicate the target face and every face sharing one of its edges.
	 * This preserves boundary/mated/seam trim types and the already validated
	 * neighboring loop classifications while keeping each candidate
	 * validation proportional to the local topology instead of the complete
	 * solid.  Only the target face's competing loops are trial mutations:
	 * changing a neighboring outer seam loop to inner makes that otherwise
	 * valid context structurally invalid before the target choice is tested. */
	std::set<int> adjacent_face_indices;
	adjacent_face_indices.insert(ambiguous->face_index);
	const ON_BrepFace &source_face = brep->m_F[ambiguous->face_index];
	for (int fli = 0; fli < source_face.m_li.Count(); ++fli) {
	    const int li = source_face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		return false;
	    const ON_BrepLoop &loop = brep->m_L[li];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!edge)
		    continue;
		for (int eti = 0; eti < edge->m_ti.Count(); ++eti) {
		    const ON_BrepTrim *use = brep->Trim(edge->m_ti[eti]);
		    const ON_BrepFace *use_face = use ? use->Face() : NULL;
		    if (use_face && use_face->m_face_index >= 0)
			adjacent_face_indices.insert(use_face->m_face_index);
		}
	    }
	}
	std::vector<int> adjacent_faces(adjacent_face_indices.begin(),
	    adjacent_face_indices.end());
	std::unique_ptr<ON_Brep> local_candidate(brep->DuplicateFaces(
	    static_cast<int>(adjacent_faces.size()), adjacent_faces.data(),
	    false));
	if (!local_candidate) {
	    wrapper->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Warning, entity_id,
		entity_type, "face_bound",
		"could not duplicate the local face neighborhood for bounded FACE_BOUND validation");
	    return false;
	}

	int local_face_index = -1;
	for (int fi = 0; fi < local_candidate->m_F.Count(); ++fi) {
	    if (local_candidate->m_F[fi].m_face_user.i ==
		    ambiguous->face_index) {
		local_face_index = fi;
		break;
	    }
	}
	if (local_face_index < 0)
	    return false;

	/* DuplicateFaces records source loop array indices in m_loop_user.i.
	 * Translate the stable original candidates once; SortFaceLoops changes
	 * face order but never these loop array indices. */
	std::map<int, int> local_loop_by_source;
	for (int li = 0; li < local_candidate->m_L.Count(); ++li)
	    local_loop_by_source[local_candidate->m_L[li].m_loop_user.i] = li;
	std::vector<int> local_candidate_loops;
	for (std::vector<int>::const_iterator source_loop =
		ambiguous->candidate_loops.begin();
		source_loop != ambiguous->candidate_loops.end(); ++source_loop) {
	    const std::map<int, int>::const_iterator local =
		local_loop_by_source.find(*source_loop);
	    if (local == local_loop_by_source.end())
		return false;
	    local_candidate_loops.push_back(local->second);
	}

	ON_BrepFace &trial_face = local_candidate->m_F[local_face_index];
	for (int fli = 0; fli < trial_face.m_li.Count(); ++fli) {
	    const int li = trial_face.m_li[fli];
	    if (li >= 0 && li < local_candidate->m_L.Count() &&
		    local_candidate->m_L[li].m_type != ON_BrepLoop::slit)
		local_candidate->m_L[li].m_type = ON_BrepLoop::inner;
	}

	size_t valid_choices = 0;
	int selected_outer_loop = -1;
	for (size_t choice = 0; choice < local_candidate_loops.size();
		++choice) {
	    if (brlcad::PullbackWorkCancelled())
		return false;
	    ++completed_candidates;
	    std::ostringstream detail;
	    detail << entity_type << " #" << entity_id << " face F"
		<< ambiguous->face_index << " FACE_BOUND alternative "
		<< choice + 1 << '/' << local_candidate_loops.size();
	    wrapper->SetProgressDetail(
		"testing supplied FACE_BOUND interpretations", entity_id,
		static_cast<uint64_t>(completed_candidates),
		static_cast<uint64_t>(candidate_count), "candidates",
		detail.str());

	    for (std::vector<int>::const_iterator li =
		    local_candidate_loops.begin();
		    li != local_candidate_loops.end(); ++li)
		if (local_candidate->m_L[*li].m_type != ON_BrepLoop::slit)
		    local_candidate->m_L[*li].m_type = ON_BrepLoop::inner;
	    const int local_outer = local_candidate_loops[choice];
	    local_candidate->m_L[local_outer].m_type = ON_BrepLoop::outer;
	    ON_BrepFace &local_face =
		local_candidate->m_F[local_face_index];
	    local_candidate->SortFaceLoops(local_face);

	    std::string unsafe_topology;
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    const bool references_safe =
		brep_topology_references_are_safe(local_candidate.get(),
		    &unsafe_topology);
	    const bool structurally_valid =
		references_safe && local_candidate->IsValid(&validation_log);
	    if (!structurally_valid) {
		if (wrapper->Verbose()) {
		    ON_String text(validation_messages);
		    std::cerr << entity_type << " #" << entity_id << ": face F"
			<< ambiguous->face_index << " FACE_BOUND choice L"
			<< ambiguous->candidate_loops[choice]
			<< " rejected: "
			<< (!references_safe ? unsafe_topology : text.Array())
			<< std::endl;
		}
		continue;
	    }
	    ++valid_choices;
	    selected_outer_loop = ambiguous->candidate_loops[choice];
	    if (valid_choices > 1)
		break;
	}
	if (valid_choices > 1) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		entity_id, entity_type, "face_bound",
		"multiple supplied FACE_BOUND interpretations were locally valid; modeling intent remained ambiguous");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": supplied FACE_BOUND alternatives remained ambiguous "
		       "on F" << ambiguous->face_index << std::endl;
	    return false;
	}
	if (valid_choices != 1 || selected_outer_loop < 0) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		entity_id, entity_type, "face_bound",
		"no supplied FACE_BOUND interpretation was locally valid; no classification repair was applied");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": no supplied FACE_BOUND alternative was valid on F"
		    << ambiguous->face_index << std::endl;
	    return false;
	}
	unique_outer_loops.push_back(selected_outer_loop);
    }

    /* Assemble the independently proven choices transactionally, then require
     * the complete OpenNURBS structure and unchanged edge-use topology to form
     * one valid solid before committing anything to the caller's BREP. */
    std::unique_ptr<ON_Brep> unique_candidate(new ON_Brep(*brep));
    for (size_t face_offset = 0; face_offset < ambiguous_faces.size();
	    ++face_offset) {
	const AmbiguousFace &ambiguous = ambiguous_faces[face_offset];
	if (ambiguous.face_index < 0 ||
		ambiguous.face_index >= unique_candidate->m_F.Count())
	    return false;
	ON_BrepFace &face = unique_candidate->m_F[ambiguous.face_index];
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= unique_candidate->m_L.Count())
		return false;
	    if (unique_candidate->m_L[li].m_type != ON_BrepLoop::slit)
		unique_candidate->m_L[li].m_type =
		    li == unique_outer_loops[face_offset] ?
		    ON_BrepLoop::outer : ON_BrepLoop::inner;
	}
	unique_candidate->SortFaceLoops(face);
    }
    std::string unsafe_topology;
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    bool candidate_oriented = false;
    bool candidate_has_boundary = true;
    const bool candidate_manifold = unique_candidate->IsManifold(
	&candidate_oriented, &candidate_has_boundary);
    if (!brep_topology_references_are_safe(unique_candidate.get(),
	    &unsafe_topology) ||
	    !unique_candidate->IsValid(&validation_log) ||
	    (require_solid &&
		(!candidate_manifold || candidate_has_boundary))) {
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    entity_id, entity_type, "face_bound",
	    "locally unique supplied FACE_BOUND interpretations did not form a "
	    "valid closed manifold; no classification repair was applied");
	return false;
    }

    if (wrapper->Verbose()) {
	std::cerr << entity_type << " #" << entity_id
	    << ": selected unique whole-BREP FACE_BOUND interpretation";
	for (size_t face_offset = 0; face_offset < ambiguous_faces.size();
		++face_offset) {
	    const int li = unique_outer_loops[face_offset];
	    std::cerr << " F" << ambiguous_faces[face_offset].face_index
		<< "->L" << li;
	    if (li >= 0 && li < unique_candidate->m_L.Count() &&
		    unique_candidate->m_L[li].m_loop_user.i > 0)
		std::cerr << "/STEP" << unique_candidate->m_L[li].m_loop_user.i;
	}
	std::cerr << " after " << completed_candidates
	    << " local candidate validation(s)" << std::endl;
    }
    *brep = *unique_candidate;
    wrapper->RecordRepair(entity_id, entity_type, "face_bound",
	"selected the unique whole-BREP interpretation of ambiguous supplied FACE_BOUND loops");
    return true;
}

} /* namespace step_brep_detail */
