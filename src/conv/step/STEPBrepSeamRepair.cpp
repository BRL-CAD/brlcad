/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepSeamRepair.cpp
 *
 * Closed-surface seam, pcurve, and singular-trim repair transactions.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

/* Return the smallest densely verified local tolerance which lets an existing
 * pcurve represent its immutable 3-D edge, subject to the same feature- and
 * item-scale ceilings used by safe regeneration.  This function only
 * measures; callers decide whether a complete repair transaction warrants
 * changing any OpenNURBS tolerances or reporting the source discrepancy. */
static double
measured_source_pcurve_tolerance(const ON_BrepTrim &trim,
	const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance, const ON_Brep *brep)
{
    const ON_Curve *source_curve = trim.TrimCurveOf();
    if (!source_curve || !surface || !(tolerance > 0.0))
	return tolerance;

    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge_nurbs.Domain();
    if (!trim_domain.IsIncreasing() || !edge_domain.IsIncreasing())
	return tolerance;

    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep &&
	brep->GetBoundingBox(item_bounds, false) && item_bounds.IsValid() ?
	item_bounds.Diagonal().Length() : 0.0;
    const double limit = std::max(tolerance,
	std::max(edge_scale * kRegenerationMaximumRelativeMismatch,
	    item_scale * kRegenerationMaximumRelativeItemMismatch));

    double measured = 0.0;
    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
	    return tolerance;
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	const ON_3dPoint lift = uv.IsValid() ?
	    surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
	const ON_3dPoint corresponding = edge_nurbs.PointAt(
	    edge_domain.ParameterAt(trim.m_bRev3d ?
		1.0 - fraction : fraction));
	if (!lift.IsValid() || !corresponding.IsValid())
	    return tolerance;

	double distance = lift.DistanceTo(corresponding);
	/* Direct parameter correspondence is an upper bound, but differently
	 * parameterized curves can describe the same locus. */
	if (distance > std::max(tolerance, measured)) {
	    double closest_parameter = 0.0;
	    if (!ON_NurbsCurve_GetClosestPoint(&closest_parameter,
		    &edge_nurbs, lift))
		return tolerance;
	    const ON_3dPoint closest = edge_nurbs.PointAt(closest_parameter);
	    if (!closest.IsValid())
		return tolerance;
	    distance = std::min(distance, lift.DistanceTo(closest));
	}
	measured = std::max(measured, distance);
	if (measured * kRegenerationToleranceSafety > limit)
	    return tolerance;
    }
    if (!(measured > tolerance))
	return tolerance;

    const double adjusted = measured * kRegenerationToleranceSafety;
    return adjusted <= limit ? adjusted : tolerance;
}

size_t
repair_closed_surface_seam_crossings(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	const std::set<int> *validated_trim_loci,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;
    size_t repaired = 0;
    std::set<int> repaired_surfaces;
    std::set<std::pair<int, int> > attempted_surface_directions;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop &loop = brep->m_L[li];
	ON_BrepFace *face = loop.Face();
	if (face_source_tags && (!face || face_source_tags->find(
		face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || repaired_surfaces.find(face->m_si) != repaired_surfaces.end())
	    continue;
	/* All trims in this loop use the same face surface.  Reuse its immutable
	 * span/bounding-box preparation for every edge projection, but release it
	 * before moving to the next loop so large multi-surface solids retain a
	 * bounded cache. */
	brlcad::PullbackContext pullback_context;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *current = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!current || !next)
		continue;
	    const ON_3dPoint end = current->PointAtEnd();
	    const ON_3dPoint start = next->PointAtStart();
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		/* With no caller-requested seam, relocation derives its candidate
		 * from every exact edge using this surface.  The result and its
		 * transactional validation are therefore identical for every loop
		 * on the same surface/direction.  A failed attempt on a giant
		 * multi-loop face must not reproject and regenerate the complete
		 * surface once for each later loop. */
		const std::pair<int, int> attempt(face->m_si, direction);
		if (attempted_surface_directions.find(attempt) !=
			attempted_surface_directions.end())
		    continue;
		const double period = surface->Domain(direction).Length();
		const double guard = std::max(ON_ZERO_TOLERANCE, period * 1.0e-8);
		if (!(period > ON_ZERO_TOLERANCE))
		    continue;
		/* Prefer the exact endpoint-image proof before projecting dozens of
		 * interior edge samples.  Both branches authorize the identical
		 * transaction, but ordinary STEP periodic faces usually expose their
		 * seam crossing directly as a one-period jump at a shared topology
		 * vertex.  Large AP242 assemblies can contain thousands of those
		 * joins; running the closest-point proof first needlessly dominates
		 * import time. */
		bool crossing = false;
		double requested_seam =
		    std::numeric_limits<double>::quiet_NaN();
		if (current->m_type != ON_BrepTrim::seam &&
			    next->m_type != ON_BrepTrim::seam &&
			    current->m_vi[1] >= 0 &&
			    current->m_vi[1] == next->m_vi[0] &&
			    current->m_vi[1] < brep->m_V.Count() &&
			    fabs(fabs(end[direction] - start[direction]) - period) <=
				guard) {
			const ON_3dPoint end_lift = surface->PointAt(end.x, end.y);
			const ON_3dPoint start_lift = surface->PointAt(start.x, start.y);
			const ON_3dPoint &vertex =
			    brep->m_V[current->m_vi[1]].point;
			double join_tolerance = std::max(LocalUnits::tolerance,
			    brep->m_V[current->m_vi[1]].m_tolerance);
			if (current->Edge())
			    join_tolerance = std::max(join_tolerance,
				current->Edge()->m_tolerance);
			if (next->Edge())
			    join_tolerance = std::max(join_tolerance,
				next->Edge()->m_tolerance);
			join_tolerance = std::max(join_tolerance,
			    std::max(current->m_tolerance[0],
				current->m_tolerance[1]));
			join_tolerance = std::max(join_tolerance,
			    std::max(next->m_tolerance[0],
				next->m_tolerance[1]));
			/* Ordinary trims whose adjacent endpoint images differ by
			 * exactly one surface period are a proven seam crossing only
			 * when both literal boundary lifts already agree with their
			 * shared STEP vertex.  Such a jump cannot form a Euclidean
			 * OpenNURBS loop; moving the private surface seam is the exact
			 * representation repair.  A mismatched or invalid lift must not
			 * authorize that topology change. */
		    crossing = end_lift.IsValid() && start_lift.IsValid() &&
			    end_lift.DistanceTo(vertex) <= join_tolerance &&
			    start_lift.DistanceTo(vertex) <= join_tolerance &&
			    end_lift.DistanceTo(start_lift) <= join_tolerance;
		    /* A horn-torus profile can place its sole collapsed pole on
		     * both native profile boundaries.  Every profile phase is then
		     * occupied, so the ordinary empty-interval seam search cannot
		     * succeed.  Prove both complete boundary isocurves collapse to
		     * this one STEP vertex and request the exact opposite half-period
		     * seam.  The relocation remains transactional over all affected
		     * trims and can reject an edge which cannot be represented there. */
		    const ON_RevSurface *revolution =
			ON_RevSurface::Cast(const_cast<ON_Surface *>(surface));
		    const int profile_direction = revolution ?
			(revolution->m_bTransposed ? 0 : 1) : -1;
		    const int angle_direction = revolution ?
			(revolution->m_bTransposed ? 1 : 0) : -1;
		    const ON_Interval profile_domain = surface->Domain(direction);
		    const double boundary_guard = std::max(ON_ZERO_TOLERANCE,
			profile_domain.Length() * 1.0e-10);
		    const bool opposite_profile_boundaries = crossing && revolution &&
			direction == profile_direction &&
			((fabs(end[direction] - profile_domain.Min()) <=
			      boundary_guard &&
			  fabs(start[direction] - profile_domain.Max()) <=
			      boundary_guard) ||
			 (fabs(end[direction] - profile_domain.Max()) <=
			      boundary_guard &&
			  fabs(start[direction] - profile_domain.Min()) <=
			      boundary_guard));
		    bool collapsed_profile_boundaries =
			opposite_profile_boundaries && angle_direction >= 0;
		    const ON_Interval angle_domain = angle_direction >= 0 ?
			surface->Domain(angle_direction) : ON_Interval::EmptyInterval;
		    for (int sample = 0; collapsed_profile_boundaries &&
			    sample <= 128; ++sample) {
			const double angle = angle_domain.ParameterAt(
			    static_cast<double>(sample) / 128.0);
			for (int side = 0; side < 2; ++side) {
			    ON_3dPoint uv;
			    uv[direction] = profile_domain[side];
			    uv[angle_direction] = angle;
			    uv.z = 0.0;
			    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			    collapsed_profile_boundaries = lift.IsValid() &&
				lift.DistanceTo(vertex) <= join_tolerance;
			    if (!collapsed_profile_boundaries)
				break;
			}
		    }
		    if (collapsed_profile_boundaries)
			requested_seam = profile_domain.Mid();
		}
		if (!crossing)
		    crossing = exact_open_trim_crosses_native_seam(*current,
			surface, direction, LocalUnits::tolerance,
			pullback_context);
		if (!crossing)
		    crossing = exact_open_trim_has_spurious_periodic_winding(
			*current, surface, direction, LocalUnits::tolerance,
			pullback_context);
		if (!crossing)
		    continue;
		attempted_surface_directions.insert(attempt);
		std::string failure;
		const bool relocated = relocate_closed_surface_loop_seam(brep, li,
		    direction, LocalUnits::tolerance, wrapper, entity_id,
		    entity_type, &failure, requested_seam,
		    validated_trim_loci);
		if (relocated) {
		    repaired_surfaces.insert(face->m_si);
		    ++repaired;
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"relocated a closed surface seam outside an exact face boundary");
		} else if (wrapper->Verbose() && !failure.empty()) {
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< " ordinary seam relocation rejected: " << failure << std::endl;
		}
		break;
	    }
	    if (repaired_surfaces.find(face->m_si) != repaired_surfaces.end())
		break;
	}
    }
    return repaired;
}


void
repair_seam_pair_from_exact_edge(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> *additional_repaired_loops,
	std::vector<int> *aligned_surface_loops,
	bool allow_surface_alignment, const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const auto solve_boundary_parameter = [](const ON_Surface *surface,
	int fixed_direction, double boundary, const ON_3dPoint &target,
	double seed, double tolerance, double *result) {
	if (!surface || !result)
	    return false;
	const int varying_direction = 1 - fixed_direction;
	const ON_Interval varying_domain = surface->Domain(varying_direction);
	if (!varying_domain.IsIncreasing())
	    return false;
	const auto refine = [surface, fixed_direction, varying_direction, boundary, &target,
	    tolerance, &varying_domain](double initial, double *refined,
	    double *refined_distance) {
	    double parameter = initial;
	    double best_distance = DBL_MAX;
	    for (int iteration = 0; iteration < 32; ++iteration) {
		const double u = fixed_direction == 0 ? boundary : parameter;
		const double v = fixed_direction == 0 ? parameter : boundary;
		ON_3dPoint point;
		ON_3dVector du, dv;
		if (!closed_surface_ev1der(surface, ON_3dPoint(u, v, 0.0),
			point, du, dv))
		    break;
		const double distance = point.DistanceTo(target);
		if (distance < best_distance)
		    best_distance = distance;
		if (distance <= tolerance) {
		    *refined = parameter;
		    *refined_distance = distance;
		    return true;
		}
		const ON_3dVector tangent = fixed_direction == 0 ? dv : du;
		const double denominator = tangent * tangent;
		if (!(denominator > ON_ZERO_TOLERANCE))
		    break;
		const ON_3dVector residual = point - target;
		double step = -(residual * tangent) / denominator;
		const double trust = 0.25 * varying_domain.Length();
		step = std::max(-trust, std::min(trust, step));
		bool accepted = false;
		for (int line_search = 0; line_search < 8; ++line_search) {
		    double candidate = parameter + step;
		    if (surface->IsClosed(varying_direction)) {
			const double period = varying_domain.Length();
			candidate += round((parameter - candidate) / period) * period;
		    } else {
			candidate = std::max(varying_domain.Min(),
			    std::min(varying_domain.Max(), candidate));
		    }
		    const ON_3dPoint candidate_uv = fixed_direction == 0 ?
			ON_3dPoint(boundary, candidate, 0.0) :
			ON_3dPoint(candidate, boundary, 0.0);
		    const ON_3dPoint candidate_point =
			closed_surface_point_at(surface, candidate_uv);
		    if (candidate_point.IsValid() &&
			    candidate_point.DistanceTo(target) < distance) {
			parameter = candidate;
			accepted = true;
			break;
		    }
		    step *= 0.5;
		}
		if (!accepted)
		    break;
	    }
	    const ON_3dPoint final_uv = fixed_direction == 0 ?
		ON_3dPoint(boundary, parameter, 0.0) :
		ON_3dPoint(parameter, boundary, 0.0);
	    const ON_3dPoint final_point =
		closed_surface_point_at(surface, final_uv);
	    const double final_distance = final_point.IsValid() ?
		final_point.DistanceTo(target) : DBL_MAX;
	    if (final_distance < best_distance)
		best_distance = final_distance;
	    *refined = parameter;
	    *refined_distance = best_distance;
	    return final_distance <= tolerance;
	};

	double parameter = seed;
	double distance = DBL_MAX;
	if (refine(seed, &parameter, &distance)) {
	    *result = parameter;
	    return true;
	}

	/* The local seed may be on a different periodic branch.  Find the best
	 * boundary interval deterministically, then give Newton one bounded retry. */
	double boundary_seed = varying_domain.Min();
	double boundary_distance = DBL_MAX;
	for (int sample = 0; sample <= kBoundaryParameterSearchSegments; ++sample) {
	    const double candidate = varying_domain.ParameterAt(
		static_cast<double>(sample) / kBoundaryParameterSearchSegments);
	    const ON_3dPoint candidate_uv = fixed_direction == 0 ?
		ON_3dPoint(boundary, candidate, 0.0) :
		ON_3dPoint(candidate, boundary, 0.0);
	    const ON_3dPoint point =
		closed_surface_point_at(surface, candidate_uv);
	    const double candidate_distance = point.IsValid() ?
		point.DistanceTo(target) : DBL_MAX;
	    if (candidate_distance < boundary_distance) {
		boundary_distance = candidate_distance;
		boundary_seed = candidate;
	    }
	}
	if (refine(boundary_seed, &parameter, &distance)) {
	    *result = parameter;
	    return true;
	}
	return false;
    };

    const auto make_candidate = [brep, &solve_boundary_parameter](
	const ON_BrepTrim &trim, const ON_BrepEdge &edge, const ON_Surface *surface,
	int fixed_direction, double boundary, double tolerance,
	const ON_3dPoint *desired_start, const ON_3dPoint *desired_end,
	int sample_count, double *score, std::string *failure_reason) -> ON_Curve * {
	if (failure_reason)
	    failure_reason->clear();
	if (sample_count < 2 || ((desired_start == NULL) != (desired_end == NULL)))
	    return NULL;
	ON_3dPointArray points;
	ON_SimpleArray<double> parameters;
	points.Reserve(sample_count + 1);
	parameters.Reserve(sample_count + 1);
	const ON_Interval trim_domain = trim.Domain();
	const ON_Interval edge_domain = edge.Domain();
	const ON_Interval varying_domain = surface->Domain(1 - fixed_direction);
	brlcad::PullbackContext fallback_context;
	double previous_parameter = ON_UNSET_VALUE;
	*score = 0.0;
	for (int sample = 0; sample <= sample_count; ++sample) {
	    const double fraction = static_cast<double>(sample) / sample_count;
	    const double edge_fraction = trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    const ON_3dPoint original_uv = trim.PointAt(
		trim_domain.ParameterAt(fraction));
	    ON_3dPoint exact_endpoint;
	    const bool use_exact_endpoint = desired_start &&
		(sample == 0 || sample == sample_count);
	    if (use_exact_endpoint) {
		exact_endpoint = sample == 0 ? *desired_start : *desired_end;
		const ON_3dPoint endpoint_lift =
		    closed_surface_point_at(surface, exact_endpoint);
		if (!endpoint_lift.IsValid() || endpoint_lift.DistanceTo(target) > tolerance ||
			fabs(exact_endpoint[fixed_direction] - boundary) > ON_ZERO_TOLERANCE) {
		    if (failure_reason)
			*failure_reason = "adjacent exact endpoint failed boundary validation";
		    return NULL;
		}
	    }
	    const double expected_parameter = desired_start ?
		(1.0 - fraction) * (*desired_start)[1 - fixed_direction] +
		fraction * (*desired_end)[1 - fixed_direction] :
		original_uv[1 - fixed_direction];
	    double seed = expected_parameter;
	    if (!desired_start && sample > 0 && surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		seed += round((previous_parameter - seed) / period) * period;
	    }
	    double varying_parameter = use_exact_endpoint ?
		exact_endpoint[1 - fixed_direction] : seed;
	    if (use_exact_endpoint && sample > 0 &&
		    surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		if (period > ON_ZERO_TOLERANCE)
		    varying_parameter += round((previous_parameter - varying_parameter) /
			period) * period;
	    }
	    const double solve_tolerance = std::max(1.0e-10, 0.1 * tolerance);
	    if (!use_exact_endpoint && !solve_boundary_parameter(surface, fixed_direction, boundary,
		    target, seed, solve_tolerance, &varying_parameter)) {
		ON_2dPoint fallback_uv(seed, seed);
		ON_3dPoint fallback_lift;
		double fallback_distance = DBL_MAX;
		if (!fallback_context.SurfaceClosestPoint(surface, target, fallback_uv,
			fallback_lift, fallback_distance, 0, solve_tolerance,
			tolerance) || fallback_distance > tolerance ||
			!solve_boundary_parameter(surface, fixed_direction, boundary,
			    target, fallback_uv[1 - fixed_direction], solve_tolerance,
			    &varying_parameter)) {
		    if (failure_reason)
			*failure_reason = "boundary solve failed at sample " +
			    std::to_string(sample);
		    return NULL;
		}
	    }
	    if (!use_exact_endpoint && surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		if (sample > 0)
		    varying_parameter += round((previous_parameter - varying_parameter) /
			period) * period;
	    }
	    ON_3dPoint uv;
	    uv[fixed_direction] = boundary;
	    uv[1 - fixed_direction] = varying_parameter;
	    uv.z = 0.0;
	    const double curve_parameter = trim_domain.ParameterAt(fraction);
	    if (points.Count() > 0 && uv.IsCoincident(points[points.Count() - 1])) {
		/* ON_PolylineCurve rejects coincident adjacent points.  Retain the
		 * latest edge parameter so a run of solver-equivalent samples does
		 * not change the remaining samples' edge/UV correspondence. */
		points[points.Count() - 1] = uv;
		parameters[parameters.Count() - 1] = curve_parameter;
	    } else {
		points.Append(uv);
		parameters.Append(curve_parameter);
	    }
	    *score += fabs(original_uv[fixed_direction] - boundary) /
		std::max(surface->Domain(fixed_direction).Length(), ON_ZERO_TOLERANCE);
	    previous_parameter = varying_parameter;
	}
	/* Closest-point and boundary solves may return equivalent values on
	 * opposite sides of a periodic parameter cut.  Normalize the completed
	 * chain cumulatively before monotonic iso classification; this changes no
	 * surface lift and removes a single artificial full-period jump. */
	if (surface->IsClosed(1 - fixed_direction)) {
	    const int varying_direction = 1 - fixed_direction;
	    const double period = varying_domain.Length();
	    if (period > ON_ZERO_TOLERANCE) {
		for (int point = 1; point < points.Count(); ++point)
		    points[point][varying_direction] += round((
			points[point - 1][varying_direction] -
			points[point][varying_direction]) / period) * period;

		/* The cumulative unwrap above deliberately works on the infinite
		 * periodic cover.  A final OpenNURBS trim cannot remain there:
		 * ON_Surface::PointAt() evaluates its literal parameters and some
		 * analytic surfaces extrapolate rather than reduce them modulo the
		 * period.  Select the unique whole-period translation that contains
		 * the complete curve in the native varying domain.  If no such
		 * translation exists, the edge crosses this second surface seam and
		 * must be split instead of being accepted through modulo-aware
		 * validation. */
		double minimum = DBL_MAX;
		double maximum = -DBL_MAX;
		for (int point = 0; point < points.Count(); ++point) {
		    minimum = std::min(minimum, points[point][varying_direction]);
		    maximum = std::max(maximum, points[point][varying_direction]);
		}
		const double parameter_slack = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    period * 1.0e-12);
		const double minimum_shift = ceil((
		    varying_domain.Min() - minimum - parameter_slack) / period);
		const double maximum_shift = floor((
		    varying_domain.Max() - maximum + parameter_slack) / period);
		if (minimum_shift > maximum_shift) {
		    if (failure_reason)
			*failure_reason =
			    "candidate crossed the native varying-parameter domain";
		    return NULL;
		}
		double period_shift = round((
		    varying_domain.Mid() - 0.5 * (minimum + maximum)) / period);
		period_shift = std::max(minimum_shift,
		    std::min(maximum_shift, period_shift));
		for (int point = 0; point < points.Count(); ++point) {
		    double &parameter = points[point][varying_direction];
		    parameter += period_shift * period;
		    if (fabs(parameter - varying_domain.Min()) <= parameter_slack)
			parameter = varying_domain.Min();
		    else if (fabs(parameter - varying_domain.Max()) <= parameter_slack)
			parameter = varying_domain.Max();
		    if (parameter < varying_domain.Min() ||
			    parameter > varying_domain.Max()) {
			if (failure_reason)
			    *failure_reason =
				"candidate could not be normalized to the native varying-parameter domain";
			return NULL;
		    }
		}
	    }
	}
	/* A closest-point solve on a highly compressed boundary can oscillate by a
	 * few parameter ulps even though the exact edge proceeds monotonically.
	 * Drop only non-progressing interior samples; the dense lift validation
	 * below proves that the resulting interpolation remains on the exact edge
	 * within model uncertainty. */
	if (points.Count() > 2) {
	    const int varying_direction = 1 - fixed_direction;
	    const double overall_delta = points[points.Count() - 1][varying_direction] -
		points[0][varying_direction];
	    if (fabs(overall_delta) > ON_ZERO_TOLERANCE) {
		ON_3dPointArray monotone_points;
		ON_SimpleArray<double> monotone_parameters;
		monotone_points.Reserve(points.Count());
		monotone_parameters.Reserve(parameters.Count());
		monotone_points.Append(points[0]);
		monotone_parameters.Append(parameters[0]);
		for (int point = 1; point + 1 < points.Count(); ++point) {
		    const double progress = (points[point][varying_direction] -
			monotone_points[monotone_points.Count() - 1][varying_direction]) *
			overall_delta;
		    if (progress <= ON_ZERO_TOLERANCE)
			continue;
		    monotone_points.Append(points[point]);
		    monotone_parameters.Append(parameters[point]);
		}
		monotone_points.Append(points[points.Count() - 1]);
		monotone_parameters.Append(parameters[parameters.Count() - 1]);
		points = monotone_points;
		parameters = monotone_parameters;
	    }
	}
	if (points.Count() < 2 || parameters.Count() != points.Count()) {
	    if (failure_reason)
		*failure_reason = "candidate samples collapsed to an invalid domain";
	    return NULL;
	}
	parameters[0] = trim_domain.Min();
	parameters[parameters.Count() - 1] = trim_domain.Max();
	ON_PolylineCurve *candidate = new ON_PolylineCurve(points, parameters);
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    if (failure_reason)
		*failure_reason = "candidate polyline was invalid";
	    delete candidate;
	    return NULL;
	}
	if (trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "open-topology seam candidate was closed";
	    delete candidate;
	    return NULL;
	}
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = trim.m_vi[endpoint];
	    const ON_3dPoint uv = candidate->PointAt(
		trim_domain[endpoint]);
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_endpoint = edge.PointAt(edge_domain[
		trim.m_bRev3d ? 1 - endpoint : endpoint]);
	    if (vertex_index < 0 || vertex_index >= brep->m_V.Count() ||
		!lift.IsValid() || !edge_endpoint.IsValid() ||
		lift.DistanceTo(brep->m_V[vertex_index].point) > tolerance ||
		lift.DistanceTo(edge_endpoint) > tolerance) {
		if (failure_reason)
		    *failure_reason = "seam endpoint did not match its exact topology vertex";
		delete candidate;
		return NULL;
	    }
	}
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	ON_Surface::ISO expected = fixed_direction == 0 ?
	    ON_Surface::x_iso : ON_Surface::y_iso;
	if (fabs(boundary - fixed_domain.Min()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::W_iso : ON_Surface::S_iso;
	else if (fabs(boundary - fixed_domain.Max()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::E_iso : ON_Surface::N_iso;
	const ON_Surface::ISO derived_iso = surface->IsIsoparametric(
	    *candidate, &trim_domain);
	if (derived_iso != expected) {
	    if (failure_reason) {
		int reversals = 0;
		int first_reversal = -1;
		double reversal_start = ON_UNSET_VALUE;
		double reversal_end = ON_UNSET_VALUE;
		const int varying_direction = 1 - fixed_direction;
		const double overall_delta = points[points.Count() - 1][varying_direction] -
		    points[0][varying_direction];
		for (int point = 1; point < points.Count(); ++point) {
		    const double delta = points[point][varying_direction] -
			points[point - 1][varying_direction];
		    if (delta * overall_delta < -ON_ZERO_TOLERANCE) {
			if (first_reversal < 0) {
			    first_reversal = point;
			    reversal_start = points[point - 1][varying_direction];
			    reversal_end = points[point][varying_direction];
			}
			++reversals;
		    }
		}
		*failure_reason = "candidate was not monotone isoparametric (derived " +
		    std::to_string(static_cast<int>(derived_iso)) + ", expected " +
		    std::to_string(static_cast<int>(expected)) + ", varying " +
		    std::to_string(points[0][varying_direction]) + "->" +
		    std::to_string(points[points.Count() - 1][varying_direction]) +
		    ", reversals " + std::to_string(reversals) +
		    (first_reversal >= 0 ? ", first " +
			std::to_string(first_reversal) + " " +
			std::to_string(reversal_start) + "->" +
			std::to_string(reversal_end) : std::string()) + ")";
	    }
	    delete candidate;
	    return NULL;
	}
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double edge_fraction = trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint uv = candidate->PointAt(trim_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    if (!lift.IsValid() || lift.DistanceTo(target) > tolerance) {
		if (failure_reason)
		    *failure_reason = "dense lift failed at sample " +
			std::to_string(sample) + " with distance " +
			std::to_string(lift.IsValid() ? lift.DistanceTo(target) : DBL_MAX);
		delete candidate;
		return NULL;
	    }
	}
	return candidate;
    };

    const auto align_revolution_seam = [brep](const ON_BrepEdge &edge,
	const ON_BrepLoop &loop, const ON_Surface *surface, double tolerance,
	std::string *failure_reason) {
	if (failure_reason)
	    *failure_reason = "surface was not a full closed revolution";
	ON_RevSurface *revolution = ON_RevSurface::Cast(
	    const_cast<ON_Surface *>(surface));
	const ON_BrepFace *source_face = loop.Face();
	if (!revolution || !source_face ||
		fabs(revolution->m_angle.Length() - ON_2PI) > ON_ZERO_TOLERANCE)
	    return false;
	const int angle_direction = revolution->m_bTransposed ? 1 : 0;
	if (!surface->IsClosed(angle_direction)) {
	    if (failure_reason)
		*failure_reason = "revolution angle direction was not closed";
	    return false;
	}
	const ON_Interval domain = surface->Domain(angle_direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    return false;

	brlcad::PullbackContext context;
	double parameters[3] = {0.0, 0.0, 0.0};
	for (int sample = 0; sample < 3; ++sample) {
	    const double fraction = 0.25 * (sample + 1);
	    const ON_3dPoint target = edge.PointAt(edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(domain.Mid(), surface->Domain(1 - angle_direction).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * tolerance), tolerance) ||
		    distance > tolerance) {
		if (failure_reason)
		    *failure_reason = "surface closest point failed at interior sample " +
			std::to_string(sample) + " with distance " +
			std::to_string(distance);
		return false;
	    }
	    parameters[sample] = uv[angle_direction];
	    if (sample > 0)
		parameters[sample] += round((parameters[0] - parameters[sample]) /
		    period) * period;
	}
	const double minimum = std::min(parameters[0],
	    std::min(parameters[1], parameters[2]));
	const double maximum = std::max(parameters[0],
	    std::max(parameters[1], parameters[2]));
	if (maximum - minimum > std::max(tolerance, 1.0e-7 * period)) {
	    if (failure_reason)
		*failure_reason = "exact edge was not constant in revolution angle (" +
		    std::to_string(parameters[0]) + "/" +
		    std::to_string(parameters[1]) + "/" +
		    std::to_string(parameters[2]) + ")";
	    return false;
	}
	const double constant_tolerance = std::max(tolerance, 1.0e-7 * period);
	const double interior_seam = (parameters[0] + parameters[1] + parameters[2]) /
	    3.0;
	double endpoint_sum = 0.0;
	int endpoint_samples = 0;
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const ON_3dPoint target = edge.PointAt(edge.Domain()[endpoint]);
	    ON_2dPoint uv(domain.Mid(), surface->Domain(1 - angle_direction).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * tolerance), tolerance) ||
		    distance > tolerance)
		continue;
	    double endpoint_parameter = uv[angle_direction];
	    endpoint_parameter += round((interior_seam - endpoint_parameter) / period) *
		period;
	    if (fabs(endpoint_parameter - interior_seam) > constant_tolerance)
		continue;
	    endpoint_sum += endpoint_parameter;
	    ++endpoint_samples;
	}
	double seam = endpoint_samples == 2 ?
	    (endpoint_sum + parameters[1]) / 3.0 : interior_seam;
	while (seam < domain.Min()) seam += period;
	while (seam > domain.Max()) seam -= period;
	if (fabs(seam - domain.Min()) <= ON_ZERO_TOLERANCE ||
		fabs(seam - domain.Max()) <= ON_ZERO_TOLERANCE) {
	    if (failure_reason)
		*failure_reason = "exact revolution seam was already on the domain boundary";
	    return false;
	}

	ON_RevSurface candidate = *revolution;
	const double angle = revolution->m_angle.ParameterAt(
	    domain.NormalizedParameterAt(seam));
	candidate.m_angle.Set(angle, angle + ON_2PI);
	candidate.m_t.Set(domain.Min(), domain.Max());
	double shifts[2] = {0.0, 0.0};
	shifts[angle_direction] = domain.Min() - seam;
	ON_Xform transform(ON_Xform::IdentityTransformation);
	transform.m_xform[0][3] = shifts[0];
	transform.m_xform[1][3] = shifts[1];

	std::vector<int> trim_indices;
	std::vector<ON_Curve *> transformed_curves;
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    if (face.m_si != source_face->m_si)
		continue;
	    for (int fli = 0; fli < face.LoopCount(); ++fli) {
		const ON_BrepLoop *affected_loop = face.Loop(fli);
		if (!affected_loop)
		    continue;
		for (int lti = 0; lti < affected_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = affected_loop->Trim(lti);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    ON_BoundingBox curve_box;
		    if (!trim || !curve || !curve->Transform(transform) ||
			    !curve->GetBoundingBox(curve_box)) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    ON_Xform wrap_transform(ON_Xform::IdentityTransformation);
		    bool wrap_required = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!candidate.IsClosed(direction) ||
				fabs(shifts[direction]) <= ON_ZERO_TOLERANCE)
			    continue;
			const ON_Interval candidate_domain = candidate.Domain(direction);
			const double candidate_period = candidate_domain.Length();
			if (!(candidate_period > ON_ZERO_TOLERANCE))
			    continue;
			const double curve_center = 0.5 *
			    (curve_box.m_min[direction] + curve_box.m_max[direction]);
			const double wrap = round((candidate_domain.Mid() - curve_center) /
			    candidate_period) * candidate_period;
			if (fabs(wrap) <= ON_ZERO_TOLERANCE)
			    continue;
			wrap_transform.m_xform[direction][3] = wrap;
			wrap_required = true;
		    }
		    if (wrap_required) {
			if (!curve->Transform(wrap_transform)) {
			    delete curve;
			    for (std::vector<ON_Curve *>::iterator cleanup =
				    transformed_curves.begin(); cleanup != transformed_curves.end();
				 ++cleanup)
				delete *cleanup;
			    return false;
			}
		    }
		    if (!curve->ChangeDimension(2) || !curve->IsValid()) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    const ON_Interval trim_domain = trim->Domain();
		    const int samples = std::min(256,
			std::max(32, trim->SpanCount() * 4));
		    bool valid = true;
		    for (int sample = 0; sample <= samples; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = trim->PointAt(parameter);
			const ON_3dPoint transformed_uv = curve->PointAt(parameter);
			const ON_3dPoint original_lift =
			    closed_surface_point_at(surface, original_uv);
			const ON_3dPoint transformed_lift =
			    closed_surface_point_at(&candidate, transformed_uv);
			if (!original_lift.IsValid() || !transformed_lift.IsValid() ||
				original_lift.DistanceTo(transformed_lift) > tolerance) {
			    if (failure_reason)
				*failure_reason = "revolution seam transform changed an affected pcurve lift by " +
				    std::to_string(original_lift.DistanceTo(transformed_lift));
			    valid = false;
			    break;
			}
		    }
		    if (!valid) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    trim_indices.push_back(trim->m_trim_index);
		    transformed_curves.push_back(curve);
		}
	    }
	}
	std::vector<int> c2_indices;
	c2_indices.reserve(transformed_curves.size());
	for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
	     curve != transformed_curves.end(); ++curve) {
	    const int c2_index = brep->AddTrimCurve(*curve);
	    if (c2_index < 0)
		return false;
	    c2_indices.push_back(c2_index);
	}
	*revolution = candidate;
	for (size_t i = 0; i < trim_indices.size(); ++i) {
	    if (!brep->SetTrimCurve(brep->m_T[trim_indices[i]], c2_indices[i]))
		return false;
	    brep->SetTrimIsoFlags(brep->m_T[trim_indices[i]]);
	}
	return true;
    };

    const auto mirror_candidate = [](const ON_Curve &source,
	const ON_BrepTrim &source_trim, const ON_BrepTrim &target_trim,
	const ON_BrepEdge &edge, const ON_Surface *surface, int fixed_direction,
	double boundary, double tolerance) -> ON_Curve * {
	ON_Curve *candidate = source.DuplicateCurve();
	if (!candidate)
	    return NULL;
	if (source_trim.m_bRev3d != target_trim.m_bRev3d && !candidate->Reverse()) {
	    delete candidate;
	    return NULL;
	}
	ON_Xform projection(ON_Xform::IdentityTransformation);
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[fixed_direction][column] = 0.0;
	projection.m_xform[fixed_direction][3] = boundary;
	const ON_Interval target_domain = target_trim.Domain();
	if (!candidate->Transform(projection) || !candidate->ChangeDimension(2) ||
		!candidate->SetDomain(target_domain.Min(), target_domain.Max()) ||
		!candidate->IsValid()) {
	    delete candidate;
	    return NULL;
	}
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	ON_Surface::ISO expected = fixed_direction == 0 ?
	    ON_Surface::x_iso : ON_Surface::y_iso;
	if (fabs(boundary - fixed_domain.Min()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::W_iso : ON_Surface::S_iso;
	else if (fabs(boundary - fixed_domain.Max()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::E_iso : ON_Surface::N_iso;
	if (surface->IsIsoparametric(*candidate, &target_domain) != expected) {
	    delete candidate;
	    return NULL;
	}
	const ON_Interval edge_domain = edge.Domain();
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double edge_fraction = target_trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint uv = candidate->PointAt(
		target_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    if (!lift.IsValid() || lift.DistanceTo(target) > tolerance) {
		delete candidate;
		return NULL;
	    }
	}
	return candidate;
    };

    std::vector<int> repaired_seam_loops;
    if (additional_repaired_loops) {
	for (std::vector<int>::const_iterator loop = additional_repaired_loops->begin();
		loop != additional_repaired_loops->end(); ++loop) {
	    if (*loop >= 0 && *loop < brep->m_L.Count() &&
		    std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
			*loop) == repaired_seam_loops.end())
		repaired_seam_loops.push_back(*loop);
	}
    }
    /* A pole-bounded periodic loop needs the same exact isoparametric-edge
     * recovery as a repaired seam loop.  In particular, a generic pullback of
     * a spherical meridian can choose an arbitrary periodic coordinate at the
     * singular endpoint and remain inside model tolerance for its first few
     * samples.  The resulting zero-area hook is legal enough for structural
     * BREP validation but can make shaded triangulation detach the cap.  Add
     * these loops to the candidate set; the independent interior samples and
     * complete dense edge/lift proof below still decide whether any curve is
     * replaced. */
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *loop_face = loop.Face();
	if (face_source_tags && (!loop_face || face_source_tags->find(
		loop_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	bool has_singular_trim = false;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::singular) {
		has_singular_trim = true;
		break;
	    }
	}
	if (has_singular_trim && std::find(repaired_seam_loops.begin(),
		repaired_seam_loops.end(), li) == repaired_seam_loops.end())
	    repaired_seam_loops.push_back(li);
    }
    const auto interior_iso = [](ON_Surface::ISO iso) {
	return iso == ON_Surface::x_iso || iso == ON_Surface::y_iso;
    };
    const auto curve_follows_exact_edge = [brep](const ON_Curve &curve,
	    const ON_BrepTrim &trim,
	    const ON_BrepEdge &edge, const ON_Surface *surface,
	    double tolerance) {
	if (!surface || !(tolerance > 0.0))
	    return false;
	const ON_Interval trim_domain = trim.Domain();
	const ON_Interval edge_domain = edge.Domain();
	if (!trim_domain.IsIncreasing() || !edge_domain.IsIncreasing())
	    return false;
	ON_Arc edge_arc;
	const bool edge_is_arc = edge.IsArc(NULL, &edge_arc, tolerance);
	const bool edge_is_line = edge.IsLinear(tolerance);
	const ON_Line edge_line(edge.PointAtStart(), edge.PointAtEnd());
	double previous_edge_parameter = trim.m_bRev3d ?
	    edge_domain.Max() : edge_domain.Min();
	const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	    edge_domain.Length() * 1.0e-10);
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const ON_3dPoint uv = curve.PointAt(
		trim_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    double edge_parameter = edge_domain.ParameterAt(
		trim.m_bRev3d ? 1.0 - fraction : fraction);
	    const ON_3dPoint corresponding = edge.PointAt(edge_parameter);
	    double distance = lift.IsValid() && corresponding.IsValid() ?
		lift.DistanceTo(corresponding) : DBL_MAX;
	    if (!uv.IsValid() || !lift.IsValid())
		return false;
	    if (distance > tolerance && edge_is_line) {
		double line_parameter = 0.0;
		if (!edge_line.ClosestPointTo(lift, &line_parameter))
		    return false;
		line_parameter = std::max(0.0,
		    std::min(1.0, line_parameter));
		edge_parameter = edge_domain.ParameterAt(line_parameter);
		distance = lift.DistanceTo(edge_line.PointAt(line_parameter));
	    } else if (distance > tolerance && edge_is_arc) {
		double arc_parameter = 0.0;
		if (!edge_arc.ClosestPointTo(lift, &arc_parameter) ||
			!edge_arc.Domain().IsIncreasing())
		    return false;
		edge_parameter = edge_domain.ParameterAt(
		    edge_arc.Domain().NormalizedParameterAt(arc_parameter));
		distance = lift.DistanceTo(edge_arc.PointAt(arc_parameter));
	    }
	    if (!std::isfinite(distance) || distance > tolerance)
		return false;
	    if (sample > 0) {
		const double progress = trim.m_bRev3d ?
		    previous_edge_parameter - edge_parameter :
		    edge_parameter - previous_edge_parameter;
		if (progress < -parameter_guard)
		    return false;
	    }
	    previous_edge_parameter = edge_parameter;
	}
	/* A locus-only test is appropriate for a pcurve: its parameterization is
	 * independent of the 3-D edge parameterization.  Still require ordered
	 * endpoint identity so a complementary arc on the same closed source
	 * curve cannot masquerade as this STEP edge use. */
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = trim.m_vi[endpoint];
	    if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
		return false;
	    const ON_3dPoint uv = curve.PointAt(trim_domain[endpoint]);
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_endpoint = edge.PointAt(edge_domain[
		trim.m_bRev3d ? 1 - endpoint : endpoint]);
	    if (!lift.IsValid() || !edge_endpoint.IsValid() ||
		    lift.DistanceTo(edge_endpoint) > tolerance ||
		    lift.DistanceTo(brep->m_V[vertex_index].point) > tolerance)
		return false;
	}
	return true;
    };
    const auto trim_follows_exact_edge = [&curve_follows_exact_edge](
	    const ON_BrepTrim &trim, const ON_BrepEdge &edge,
	    const ON_Surface *surface, double tolerance) {
	const ON_Curve *curve = trim.TrimCurveOf();
	return curve && curve_follows_exact_edge(*curve, trim, edge, surface,
	    tolerance);
    };
    const auto loop_has_repeated_interior_pole = [brep](
	    const ON_BrepLoop &loop, const ON_Surface *surface) {
	if (!surface || !surface->IsClosed(0) || !surface->IsClosed(1) ||
		loop.TrimCount() < 4)
	    return false;
	for (int first = 0; first < loop.TrimCount(); ++first) {
	    const ON_BrepTrim *first_trim = loop.Trim(first);
	    if (!first_trim || first_trim->m_vi[0] < 0 ||
		    first_trim->m_vi[0] >= brep->m_V.Count())
		continue;
	    for (int second = first + 2; second < loop.TrimCount(); ++second) {
		if (first == 0 && second == loop.TrimCount() - 1)
		    continue;
		const ON_BrepTrim *second_trim = loop.Trim(second);
		if (!second_trim || second_trim->m_vi[0] !=
			first_trim->m_vi[0])
		    continue;
		const ON_3dPoint first_uv = first_trim->PointAtStart();
		const ON_3dPoint second_uv = second_trim->PointAtStart();
		if (!first_uv.IsValid() || !second_uv.IsValid())
		    continue;
		for (int winding_direction = 0; winding_direction < 2;
			++winding_direction) {
		    const int pole_direction = 1 - winding_direction;
		    const ON_Interval winding_domain =
			surface->Domain(winding_direction);
		    const ON_Interval pole_domain = surface->Domain(pole_direction);
		    if (!winding_domain.IsIncreasing() ||
			    !pole_domain.IsIncreasing())
			continue;
		    const double winding_guard = kPeriodicParameterSnapFraction *
			std::max(1.0, winding_domain.Length());
		    const double pole_guard = kPeriodicParameterSnapFraction *
			std::max(1.0, pole_domain.Length());
		    if (fabs(fabs(second_uv[winding_direction] -
			    first_uv[winding_direction]) -
			winding_domain.Length()) > winding_guard ||
			fabs(second_uv[pole_direction] -
			    first_uv[pole_direction]) > pole_guard)
			continue;
		    const double pole_parameter = 0.5 *
			(first_uv[pole_direction] + second_uv[pole_direction]);
		    if (fabs(pole_parameter - pole_domain.Min()) <= pole_guard ||
			    fabs(pole_parameter - pole_domain.Max()) <= pole_guard)
			continue;
		    const int vertex_index = first_trim->m_vi[0];
		    const ON_3dPoint &vertex = brep->m_V[vertex_index].point;
		    const double tolerance = std::max(LocalUnits::tolerance,
			brep->m_V[vertex_index].m_tolerance);
		    bool collapsed = vertex.IsValid();
		    for (int sample = 0; collapsed && sample <= 128; ++sample) {
			ON_3dPoint uv;
			uv[winding_direction] = winding_domain.ParameterAt(
			    static_cast<double>(sample) / 128.0);
			uv[pole_direction] = pole_parameter;
			uv.z = 0.0;
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			collapsed = lift.IsValid() &&
			    lift.DistanceTo(vertex) <= tolerance;
		    }
		    if (collapsed)
			return true;
		}
	    }
	}
	return false;
    };
    std::map<int, bool> repeated_interior_pole_loops;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_index = edge.m_ti[0];
	const int second_index = edge.m_ti[1];
	if (first_index < 0 || first_index >= brep->m_T.Count() ||
		second_index < 0 || second_index >= brep->m_T.Count())
	    continue;
	ON_BrepTrim &first = brep->m_T[first_index];
	ON_BrepTrim &second = brep->m_T[second_index];
	if (first.m_type != ON_BrepTrim::seam || second.m_type != ON_BrepTrim::seam ||
		first.m_li < 0 || first.m_li != second.m_li ||
		first.m_li >= brep->m_L.Count())
	    continue;
	const ON_BrepLoop &loop = brep->m_L[first.m_li];
	const ON_BrepFace *loop_face = loop.Face();
	if (face_source_tags && (!loop_face || face_source_tags->find(
		loop_face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *surface = loop_face ? loop_face->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	std::map<int, bool>::iterator repeated_pole =
	    repeated_interior_pole_loops.find(first.m_li);
	if (repeated_pole == repeated_interior_pole_loops.end())
	    repeated_pole = repeated_interior_pole_loops.insert(std::make_pair(
		first.m_li, loop_has_repeated_interior_pole(loop, surface))).first;
	const bool defer_to_repeated_pole_split = repeated_pole->second;

	/* A repeated STEP edge can be an interior zero-area keyhole bridge, not a
	 * periodic surface seam.  If one reciprocal use already supplies an
	 * interior isoparametric pcurve which densely follows the immutable edge,
	 * mirror that exact curve onto the damaged use.  Forcing both uses onto
	 * opposite domain boundaries invents a different torus arc; retaining the
	 * coincident forward/reverse pair lets the later keyhole normalizer remove
	 * only the zero-area bridge and preserve both bounded face components. */
	ON_BrepTrim *interior_source = NULL;
	ON_BrepTrim *interior_target = NULL;
	for (int member = 0; member < 2 && !interior_source &&
		!defer_to_repeated_pole_split; ++member) {
	    ON_BrepTrim *candidate_source = member == 0 ? &first : &second;
	    ON_BrepTrim *candidate_target = member == 0 ? &second : &first;
	    const ON_Interval candidate_domain = candidate_source->Domain();
	    const ON_Surface::ISO candidate_iso =
		candidate_domain.IsIncreasing() ?
		surface->IsIsoparametric(*candidate_source, &candidate_domain) :
		ON_Surface::not_iso;
	    double candidate_tolerance = std::max(LocalUnits::tolerance,
		edge.m_tolerance);
	    candidate_tolerance = std::max(candidate_tolerance,
		std::max(candidate_source->m_tolerance[0],
		    candidate_source->m_tolerance[1]));
	    candidate_tolerance = std::max(candidate_tolerance,
		std::max(candidate_target->m_tolerance[0],
		    candidate_target->m_tolerance[1]));
	    if (interior_iso(candidate_iso) &&
		    trim_follows_exact_edge(*candidate_source, edge, surface,
			candidate_tolerance)) {
		interior_source = candidate_source;
		interior_target = candidate_target;
	    }
	}
	if (interior_source && interior_target) {
	    const ON_Interval source_domain = interior_source->Domain();
	    const ON_Surface::ISO source_iso =
		surface->IsIsoparametric(*interior_source, &source_domain);
	    const double parameter_tolerance =
		ON_ZERO_TOLERANCE * kNumericalToleranceScale;
	    bool coincident = interior_iso(source_iso) &&
		opposite_trim_curves_coincide(&first, &second,
		    parameter_tolerance);
	    if (!coincident) {
		std::unique_ptr<ON_Curve> mirrored(
		    interior_source->DuplicateCurve());
		const ON_Interval target_domain = interior_target->Domain();
		bool valid = mirrored.get() != NULL &&
		    (interior_source->m_bRev3d == interior_target->m_bRev3d ||
			mirrored->Reverse()) &&
		    mirrored->SetDomain(target_domain.Min(), target_domain.Max()) &&
		    mirrored->ChangeDimension(2) && mirrored->IsValid() &&
		    surface->IsIsoparametric(*mirrored, &target_domain) ==
			source_iso;
		if (valid) {
		    double candidate_tolerance = std::max(
			LocalUnits::tolerance, edge.m_tolerance);
		    candidate_tolerance = std::max(candidate_tolerance,
			std::max(interior_target->m_tolerance[0],
			    interior_target->m_tolerance[1]));
		    valid = curve_follows_exact_edge(*mirrored,
			*interior_target, edge, surface, candidate_tolerance);
		}
		if (valid) {
		    ON_Curve *owned = mirrored.release();
		    const int c2_index = brep->AddTrimCurve(owned);
		    if (c2_index < 0) {
			delete owned;
			valid = false;
		    } else {
			valid = brep->SetTrimCurve(*interior_target, c2_index);
		    }
		}
		if (valid) {
		    interior_source->m_iso = source_iso;
		    interior_target->m_iso = source_iso;
		    coincident = opposite_trim_curves_coincide(&first, &second,
			parameter_tolerance);
		}
	    }
	    if (coincident) {
		if (std::find(repaired_seam_loops.begin(),
			repaired_seam_loops.end(), first.m_li) ==
			repaired_seam_loops.end())
		    repaired_seam_loops.push_back(first.m_li);
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "mirrored an exact interior keyhole pcurve onto its reciprocal STEP edge use");
		continue;
	    }
	}

	const auto is_boundary_iso = [](ON_Surface::ISO iso) {
	    return iso == ON_Surface::W_iso || iso == ON_Surface::E_iso ||
		iso == ON_Surface::S_iso || iso == ON_Surface::N_iso;
	};
	const auto complementary_boundary_isos = [](ON_Surface::ISO first_iso,
		ON_Surface::ISO second_iso) {
	    return
		(first_iso == ON_Surface::W_iso &&
		    second_iso == ON_Surface::E_iso) ||
		(first_iso == ON_Surface::E_iso &&
		    second_iso == ON_Surface::W_iso) ||
		(first_iso == ON_Surface::S_iso &&
		    second_iso == ON_Surface::N_iso) ||
		(first_iso == ON_Surface::N_iso &&
		    second_iso == ON_Surface::S_iso);
	};
	/* Refresh stale stored ISO flags before replacing any geometry.  A valid
	 * full-domain loop on a doubly periodic surface has two reciprocal seam
	 * pairs.  Its existing pcurves may already occupy complementary domain
	 * sides and densely follow the shared exact edge even when an intervening
	 * topology refresh left one m_iso generic.  Regenerating that pair can
	 * choose the other periodic direction and open the otherwise valid loop. */
	const auto measured_boundary_iso = [surface](const ON_BrepTrim &trim) {
	    const ON_Interval trim_domain = trim.Domain();
	    if (!trim_domain.IsIncreasing())
		return ON_Surface::not_iso;
	    return surface->IsIsoparametric(trim, &trim_domain);
	};
	const ON_Surface::ISO measured_first_iso =
	    measured_boundary_iso(first);
	const ON_Surface::ISO measured_second_iso =
	    measured_boundary_iso(second);
	double existing_pair_tolerance = std::max(LocalUnits::tolerance,
	    edge.m_tolerance);
	existing_pair_tolerance = std::max(existing_pair_tolerance,
	    std::max(first.m_tolerance[0], first.m_tolerance[1]));
	existing_pair_tolerance = std::max(existing_pair_tolerance,
	    std::max(second.m_tolerance[0], second.m_tolerance[1]));
	if (complementary_boundary_isos(measured_first_iso,
		measured_second_iso) &&
		trim_follows_exact_edge(first, edge, surface,
		    existing_pair_tolerance) &&
		trim_follows_exact_edge(second, edge, surface,
		    existing_pair_tolerance)) {
	    first.m_iso = measured_first_iso;
	    second.m_iso = measured_second_iso;
	    continue;
	}
	/* Generic x_iso/y_iso is not sufficient for a topological seam.
	 * OpenNURBS requires each member to lie on an explicit domain boundary,
	 * and the two uses of one same-loop seam edge must occupy complementary
	 * sides.  A periodic branch or surface-seam move can leave both otherwise
	 * exact pcurves on W (or the analogous side); their lifts and edge checks
	 * still pass, but the resulting BREP is structurally invalid. */
	bool needs_repair = !is_boundary_iso(first.m_iso) ||
	    !is_boundary_iso(second.m_iso) ||
	    !complementary_boundary_isos(first.m_iso, second.m_iso);
	const ON_BrepTrim *pair[2] = {&first, &second};
	for (int member = 0; member < 2 && !needs_repair; ++member) {
	    const ON_Interval trim_domain = pair[member]->Domain();
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const ON_3dPoint uv = pair[member]->PointAt(trim_domain[endpoint]);
		/* Inspect the literal parameters that OpenNURBS will store and
		 * evaluate.  A modulo-equivalent endpoint outside a second closed
		 * direction is still an invalid final trim and must enter the
		 * native-domain regeneration below. */
		const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		const ON_3dPoint target = edge.PointAt(edge.Domain()[
		    pair[member]->m_bRev3d ? 1 - endpoint : endpoint]);
		if (!lift.IsValid() || lift.DistanceTo(target) > LocalUnits::tolerance) {
		    needs_repair = true;
		    break;
		}
	    }
	}
	if (!needs_repair)
	    continue;

	/* A seam pair can already contain two exact reciprocal source pcurves but
	 * have both curves on the same native boundary.  This is common in older
	 * writers which rely on periodic equivalence instead of the complementary
	 * boundary representation required by OpenNURBS.  Prefer an exact
	 * one-period translation over reconstructing either curve.  Safe mode may
	 * retain a small, densely bounded source edge/surface discrepancy, but the
	 * curve translation itself must preserve every sampled surface lift at
	 * numerical precision.  Commit the curves, flags, and measured tolerance
	 * together so a failed candidate changes nothing. */
	const bool safe_measured_repair =
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	const bool same_measured_boundary =
	    measured_first_iso == measured_second_iso &&
	    is_boundary_iso(measured_first_iso) && surface->IsClosed(0) &&
	    surface->IsClosed(1);
	if (safe_measured_repair && same_measured_boundary) {
	    int fixed_direction = -1;
	    int source_side = -1;
	    if (measured_first_iso == ON_Surface::W_iso) {
		fixed_direction = 0;
		source_side = 0;
	    } else if (measured_first_iso == ON_Surface::E_iso) {
		fixed_direction = 0;
		source_side = 1;
	    } else if (measured_first_iso == ON_Surface::S_iso) {
		fixed_direction = 1;
		source_side = 0;
	    } else if (measured_first_iso == ON_Surface::N_iso) {
		fixed_direction = 1;
		source_side = 1;
	    }
	    const ON_Interval fixed_domain = fixed_direction >= 0 ?
		surface->Domain(fixed_direction) : ON_Interval::EmptyInterval;
	    const double period = fixed_domain.Length();
	    std::unique_ptr<ON_Curve> translated(second.DuplicateCurve());
	    ON_Xform shift(ON_Xform::IdentityTransformation);
	    if (fixed_direction >= 0)
		shift.m_xform[fixed_direction][3] =
		    source_side == 0 ? period : -period;
	    std::string exact_translation_failure;
	    bool translated_exactly = fixed_direction >= 0 &&
		surface->IsClosed(fixed_direction) &&
		period > ON_ZERO_TOLERANCE && translated.get() &&
		translated->Transform(shift) && translated->ChangeDimension(2) &&
		translated->IsValid() &&
		validate_periodic_trim_translation(surface, second, *translated,
		    &exact_translation_failure);
	    ON_NurbsCurve edge_nurbs;
	    double verified_tolerance = existing_pair_tolerance;
	    if (translated_exactly && edge.GetNurbForm(edge_nurbs)) {
		verified_tolerance = measured_source_pcurve_tolerance(first,
		    surface, edge_nurbs, verified_tolerance, brep);
		verified_tolerance = measured_source_pcurve_tolerance(second,
		    surface, edge_nurbs, verified_tolerance, brep);
	    } else {
		translated_exactly = false;
	    }
	    double measured_mismatch = 0.0;
	    std::string bounded_translation_failure;
	    const bool bounded_pair = translated_exactly &&
		trim_follows_exact_edge(first, edge, surface,
		    verified_tolerance) &&
		trim_follows_exact_edge(second, edge, surface,
		    verified_tolerance) &&
		validate_periodic_trim_translation(surface, second, *translated,
		    &bounded_translation_failure, verified_tolerance,
		    &measured_mismatch);
	    const ON_Interval translated_domain = translated.get() ?
		translated->Domain() : ON_Interval::EmptyInterval;
	    const ON_Surface::ISO translated_iso = bounded_pair &&
		translated_domain.IsIncreasing() ?
		surface->IsIsoparametric(*translated, &translated_domain) :
		ON_Surface::not_iso;
	    const ON_Surface::ISO expected_iso = fixed_direction == 0 ?
		(source_side == 0 ? ON_Surface::E_iso : ON_Surface::W_iso) :
		(source_side == 0 ? ON_Surface::N_iso : ON_Surface::S_iso);
	    if (bounded_pair && translated_iso == expected_iso) {
		ON_Curve *owned = translated.release();
		const int c2_index = brep->AddTrimCurve(owned);
		if (c2_index < 0) {
		    delete owned;
		} else if (brep->SetTrimCurve(second, c2_index)) {
		    first.m_iso = measured_first_iso;
		    second.m_iso = expected_iso;
		    if (verified_tolerance > existing_pair_tolerance) {
			edge.m_tolerance = std::max(edge.m_tolerance,
			    verified_tolerance);
			first.m_tolerance[0] = std::max(first.m_tolerance[0],
			    verified_tolerance);
			first.m_tolerance[1] = std::max(first.m_tolerance[1],
			    verified_tolerance);
			second.m_tolerance[0] = std::max(second.m_tolerance[0],
			    verified_tolerance);
			second.m_tolerance[1] = std::max(second.m_tolerance[1],
			    verified_tolerance);
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning,
			    entity_id, entity_type, "trim_pcurve",
			    "paired seam pcurves and their 3-D edge exceeded the "
			    "declared tolerance; used a densely measured local "
			    "OpenNURBS tolerance");
			wrapper->RecordRepair(entity_id, entity_type,
			    "trim_pcurve",
			    "adjusted one seam-pair tolerance to densely measured "
			    "source geometry");
		    }
		    wrapper->RecordRepair(entity_id, entity_type,
			"trim_pcurve",
			"translated one exact seam pcurve to the complementary "
			"periodic boundary");
		    if (std::find(repaired_seam_loops.begin(),
			    repaired_seam_loops.end(), first.m_li) ==
			    repaired_seam_loops.end())
			repaired_seam_loops.push_back(first.m_li);
		    continue;
		}
	    }
	    if (wrapper->Verbose() && translated_exactly && !bounded_pair) {
		std::cerr << entity_type << " #" << entity_id
		    << ": complementary seam translation rejected for STEP edge "
		    << edge.m_edge_user.i << ": "
		    << bounded_translation_failure << std::endl;
	    } else if (wrapper->Verbose() && !translated_exactly &&
		    !exact_translation_failure.empty()) {
		std::cerr << entity_type << " #" << entity_id
		    << ": exact complementary seam translation rejected for "
		    << "STEP edge " << edge.m_edge_user.i << ": "
		    << exact_translation_failure << std::endl;
	    }
	}
	std::string revolution_alignment_failure;
	if (align_revolution_seam(edge, loop, surface, LocalUnits::tolerance,
		&revolution_alignment_failure)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"aligned a periodic revolution surface seam with an exact edge");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": aligned revolution seam with STEP edge "
		    << edge.m_edge_user.i << std::endl;
	} else if (wrapper->Verbose()) {
	    std::cerr << entity_type << " #" << entity_id
		<< ": revolution seam alignment rejected for STEP edge "
		<< edge.m_edge_user.i << ": " << revolution_alignment_failure
		<< std::endl;
	}
	std::string surface_alignment_failure;
	if (allow_surface_alignment &&
		align_closed_surface_seam_from_trim_pair(brep, edge, loop, surface,
		LocalUnits::tolerance, &surface_alignment_failure)) {
	    if (aligned_surface_loops && std::find(aligned_surface_loops->begin(),
		    aligned_surface_loops->end(), first.m_li) ==
		    aligned_surface_loops->end())
		aligned_surface_loops->push_back(first.m_li);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"aligned a periodic surface seam with an exact edge");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": aligned periodic surface seam with STEP edge "
		    << edge.m_edge_user.i << std::endl;
	} else if (allow_surface_alignment && wrapper->Verbose()) {
	    std::cerr << entity_type << " #" << entity_id
		<< ": periodic surface seam alignment rejected for STEP edge "
		<< edge.m_edge_user.i << ": " << surface_alignment_failure
		<< std::endl;
	}

	ON_Curve *best_first = NULL;
	ON_Curve *best_second = NULL;
	double best_score = DBL_MAX;
	int best_direction = -1;
	int best_assignment = -1;
	std::ostringstream candidate_failures;
	const auto loop_closure_score = [brep, &loop, first_index, second_index](
	    const ON_Curve *first_candidate, const ON_Curve *second_candidate) {
	    double score = 0.0;
	    for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
		const int trim_index = loop.m_ti[lti];
		if (trim_index != first_index && trim_index != second_index)
		    continue;
		const int previous_index = loop.m_ti[
		    (lti + loop.m_ti.Count() - 1) % loop.m_ti.Count()];
		const int next_index = loop.m_ti[(lti + 1) % loop.m_ti.Count()];
		const ON_Curve *candidate = trim_index == first_index ?
		    first_candidate : second_candidate;
		if (!candidate)
		    return DBL_MAX;
		const ON_3dPoint previous_end = previous_index == first_index ?
		    first_candidate->PointAtEnd() : previous_index == second_index ?
		    second_candidate->PointAtEnd() :
		    brep->m_T[previous_index].PointAtEnd();
		const ON_3dPoint next_start = next_index == first_index ?
		    first_candidate->PointAtStart() : next_index == second_index ?
		    second_candidate->PointAtStart() :
		    brep->m_T[next_index].PointAtStart();
		score += candidate->PointAtStart().DistanceTo(previous_end);
		score += candidate->PointAtEnd().DistanceTo(next_start);
	    }
	    return score;
	};
	const auto adjacent_endpoints = [brep, &loop, surface](int trim_index,
	    int fixed_direction, double boundary, ON_3dPoint *start,
	    ON_3dPoint *end) {
	    if (!start || !end)
		return false;
	    int offset = -1;
	    for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
		if (loop.m_ti[lti] == trim_index) {
		    offset = lti;
		    break;
		}
	    }
	    if (offset < 0)
		return false;
	    const ON_BrepTrim *previous = loop.Trim(
		(offset + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((offset + 1) % loop.TrimCount());
	    const ON_BrepTrim *trim = brep->Trim(trim_index);
	    if (!previous || !next || !trim ||
		previous->m_vi[1] != trim->m_vi[0] ||
		trim->m_vi[1] != next->m_vi[0])
		return false;
	    *start = previous->PointAtEnd();
	    *end = next->PointAtStart();
	    const double parameter_tolerance = ON_ZERO_TOLERANCE *
		kNumericalToleranceScale;
	    if (fabs((*start)[fixed_direction] - boundary) <= parameter_tolerance &&
		    fabs((*end)[fixed_direction] - boundary) <= parameter_tolerance)
		return true;

	    /* An adjacent exact-edge pullback may stop a few parameter ulps inside
	     * a newly moved surface seam.  Project just that fixed coordinate to
	     * the boundary, and admit it only when both projected endpoints still
	     * lift to their authoritative topology vertices within model
	     * uncertainty. */
	    ON_3dPoint projected_start = *start;
	    ON_3dPoint projected_end = *end;
	    projected_start[fixed_direction] = boundary;
	    projected_end[fixed_direction] = boundary;
	    const ON_3dPoint start_lift =
		closed_surface_point_at(surface, projected_start);
	    const ON_3dPoint end_lift =
		closed_surface_point_at(surface, projected_end);
	    if (!start_lift.IsValid() || !end_lift.IsValid() ||
		trim->m_vi[0] < 0 || trim->m_vi[0] >= brep->m_V.Count() ||
		trim->m_vi[1] < 0 || trim->m_vi[1] >= brep->m_V.Count() ||
		start_lift.DistanceTo(brep->m_V[trim->m_vi[0]].point) >
		    LocalUnits::tolerance ||
		end_lift.DistanceTo(brep->m_V[trim->m_vi[1]].point) >
		    LocalUnits::tolerance)
		return false;
	    *start = projected_start;
	    *end = projected_end;
	    return true;
	};
	const auto exact_edge_endpoints = [brep, surface, &edge,
	    &solve_boundary_parameter](int trim_index, int fixed_direction,
	    double boundary, ON_3dPoint *start, ON_3dPoint *end) {
	    if (!start || !end)
		return false;
	    const ON_BrepTrim *trim = brep->Trim(trim_index);
	    if (!trim)
		return false;
	    ON_3dPoint *endpoint_uv[2] = {start, end};
	    brlcad::PullbackContext context;
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const int vertex_index = trim->m_vi[endpoint];
		if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
		    return false;
		const ON_3dPoint edge_target = edge.PointAt(edge.Domain()[
		    trim->m_bRev3d ? 1 - endpoint : endpoint]);
		const ON_3dPoint target = brep->m_V[vertex_index].point;
		if (!edge_target.IsValid() || edge_target.DistanceTo(target) >
			LocalUnits::tolerance)
		    return false;
		ON_2dPoint pulled_uv(surface->Domain(0).Mid(),
		    surface->Domain(1).Mid());
		ON_3dPoint pulled_lift;
		double distance = DBL_MAX;
		if (!context.SurfaceClosestPoint(surface, target, pulled_uv,
			pulled_lift, distance, 0,
			std::max(1.0e-10, LocalUnits::tolerance * 1.0e-6),
			LocalUnits::tolerance) || distance > LocalUnits::tolerance)
		    return false;
		double varying_parameter = pulled_uv[1 - fixed_direction];
		if (!solve_boundary_parameter(surface, fixed_direction, boundary,
			target, varying_parameter,
			std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
			&varying_parameter))
		    return false;
		(*endpoint_uv[endpoint])[fixed_direction] = boundary;
		(*endpoint_uv[endpoint])[1 - fixed_direction] = varying_parameter;
		(*endpoint_uv[endpoint]).z = 0.0;
		const ON_3dPoint lift =
		    closed_surface_point_at(surface, *endpoint_uv[endpoint]);
		if (!lift.IsValid() || lift.DistanceTo(target) >
			LocalUnits::tolerance || lift.DistanceTo(
			edge_target) > LocalUnits::tolerance)
		    return false;
	    }
	    const int varying_direction = 1 - fixed_direction;
	    if (surface->IsClosed(varying_direction)) {
		const double period = surface->Domain(varying_direction).Length();
		if (period > ON_ZERO_TOLERANCE) {
		    /* The two equivalent end branches can be exactly half a period
		     * from the start.  round() then chooses a branch arbitrarily and
		     * can introduce a single reversal at the final sample.  Pull back
		     * the immutable edge midpoint and choose the end branch that
		     * continues through it monotonically. */
		    const ON_3dPoint midpoint_target = edge.PointAt(
			edge.Domain().ParameterAt(0.5));
		    ON_2dPoint midpoint_uv(surface->Domain(0).Mid(),
			surface->Domain(1).Mid());
		    ON_3dPoint midpoint_lift;
		    double midpoint_distance = DBL_MAX;
		    double midpoint_parameter = ON_UNSET_VALUE;
		    const bool have_midpoint = context.SurfaceClosestPoint(surface,
			midpoint_target, midpoint_uv, midpoint_lift,
			midpoint_distance, 0,
			std::max(1.0e-10, LocalUnits::tolerance * 1.0e-6),
			LocalUnits::tolerance) &&
			midpoint_distance <= LocalUnits::tolerance &&
			solve_boundary_parameter(surface, fixed_direction, boundary,
			    midpoint_target, midpoint_uv[varying_direction],
			    std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
			    &midpoint_parameter);
		    if (have_midpoint) {
			midpoint_parameter += round(((*start)[varying_direction] -
			    midpoint_parameter) / period) * period;
			double best_end = (*end)[varying_direction];
			double best_branch_score = DBL_MAX;
			for (int shift = -2; shift <= 2; ++shift) {
			    const double candidate_end = (*end)[varying_direction] +
				shift * period;
			    const double first_delta = midpoint_parameter -
				(*start)[varying_direction];
			    const double second_delta = candidate_end - midpoint_parameter;
			    const bool reversal = first_delta * second_delta <
				-ON_ZERO_TOLERANCE;
			    const double score = (reversal ? 10.0 * period : 0.0) +
				fabs(first_delta - second_delta) +
				1.0e-9 * (fabs(first_delta) + fabs(second_delta));
			    if (score < best_branch_score) {
				best_branch_score = score;
				best_end = candidate_end;
			    }
			}
			(*end)[varying_direction] = best_end;
		    } else {
			(*end)[varying_direction] += round(((*start)[varying_direction] -
			    (*end)[varying_direction]) / period) * period;
		    }
		}
	    }
	    return true;
	};
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int assignment = 0; assignment < 2; ++assignment) {
		double first_score = 0.0;
		double second_score = 0.0;
		std::string first_failure;
		std::string second_failure;
		ON_3dPoint first_start, first_end, second_start, second_end;
		bool have_first_endpoints = exact_edge_endpoints(first_index,
		    direction, domain[assignment], &first_start, &first_end);
		bool have_second_endpoints = exact_edge_endpoints(second_index,
		    direction, domain[1 - assignment], &second_start, &second_end);
		if (!have_first_endpoints)
		    have_first_endpoints = adjacent_endpoints(first_index,
			direction, domain[assignment], &first_start, &first_end);
		if (!have_second_endpoints)
		    have_second_endpoints = adjacent_endpoints(second_index,
			direction, domain[1 - assignment], &second_start, &second_end);
		ON_Curve *first_candidate = make_candidate(first, edge, surface,
		    direction, domain[assignment], LocalUnits::tolerance,
		    have_first_endpoints ? &first_start : NULL,
		    have_first_endpoints ? &first_end : NULL,
		    256, &first_score, &first_failure);
		ON_Curve *second_candidate = make_candidate(second, edge, surface,
		    direction, domain[1 - assignment], LocalUnits::tolerance,
		    have_second_endpoints ? &second_start : NULL,
		    have_second_endpoints ? &second_end : NULL,
		    256, &second_score, &second_failure);
		if (!first_candidate && second_candidate) {
		    first_candidate = mirror_candidate(*second_candidate, second, first,
			edge, surface, direction, domain[assignment],
			LocalUnits::tolerance);
		    if (first_candidate) {
			first_failure.clear();
			first_score = second_score;
		    }
		}
		if (!second_candidate && first_candidate) {
		    second_candidate = mirror_candidate(*first_candidate, first, second,
			edge, surface, direction, domain[1 - assignment],
			LocalUnits::tolerance);
		    if (second_candidate) {
			second_failure.clear();
			second_score = first_score;
		    }
		}
		if (!first_candidate || !second_candidate)
		    candidate_failures << " d" << direction << 'a' << assignment
			<< " first=" << (first_failure.empty() ? "ok" : first_failure)
			<< " second=" << (second_failure.empty() ? "ok" : second_failure);
		const double closure_score = first_candidate && second_candidate ?
		    loop_closure_score(first_candidate, second_candidate) : DBL_MAX;
		const double candidate_score = closure_score +
		    1.0e-9 * (first_score + second_score);
		if (!first_candidate || !second_candidate || candidate_score >= best_score) {
		    delete first_candidate;
		    delete second_candidate;
		    continue;
		}
		delete best_first;
		delete best_second;
		best_first = first_candidate;
		best_second = second_candidate;
		best_score = candidate_score;
		best_direction = direction;
		best_assignment = assignment;
	    }
	}
	if (!best_first || !best_second) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact seam regeneration rejected for edge " << ei
		    << "/STEP" << edge.m_edge_user.i << candidate_failures.str()
		    << std::endl;
	    delete best_first;
	    delete best_second;
	    continue;
	}
	const int first_c2 = brep->AddTrimCurve(best_first);
	const int second_c2 = brep->AddTrimCurve(best_second);
	if (first_c2 < 0 || second_c2 < 0 ||
		!brep->SetTrimCurve(first, first_c2) ||
		!brep->SetTrimCurve(second, second_c2)) {
	    if (first_c2 < 0)
		delete best_first;
	    if (second_c2 < 0)
		delete best_second;
	    continue;
	}
	brep->SetTrimIsoFlags(first);
	brep->SetTrimIsoFlags(second);
	/* The candidates were constructed on opposite exact domain boundaries and
	 * densely validated against the same STEP edge.  Preserve that proof in
	 * the topology flags explicitly.  SetTrimIsoFlags() can return a generic
	 * or stale flag for one member when the surface has a collapsed derivative
	 * at an endpoint, which makes an otherwise valid seam pair fail
	 * ON_Brep::IsValid(). */
	if (best_direction == 0 && best_assignment >= 0) {
	    first.m_iso = best_assignment == 0 ? ON_Surface::W_iso :
		ON_Surface::E_iso;
	    second.m_iso = best_assignment == 0 ? ON_Surface::E_iso :
		ON_Surface::W_iso;
	} else if (best_direction == 1 && best_assignment >= 0) {
	    first.m_iso = best_assignment == 0 ? ON_Surface::S_iso :
		ON_Surface::N_iso;
	    second.m_iso = best_assignment == 0 ? ON_Surface::N_iso :
		ON_Surface::S_iso;
	}
	if (std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
		first.m_li) == repaired_seam_loops.end())
	    repaired_seam_loops.push_back(first.m_li);
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated paired seam pcurves from the exact edge");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": regenerated seam pair "
		<< first_index << '/' << second_index << " from exact STEP edge "
		<< edge.m_edge_user.i << std::endl;
    }

    /* An exact isoparametric 3D edge on a periodic surface is frequently
     * supplied with a diagonal or slightly displaced pcurve.  Do not rely on
     * ON_BrepEdge::IsClosed(): imported circle proxies can have a closed STEP
     * topology without reporting closed here, and open isoparametric edges
     * need the same treatment.  Limit this repair to loops whose seam pair was
     * regenerated, require an actual p-space closure defect (or an unclassified
     * pcurve), prove a constant surface parameter with independent interior
     * samples, and densely validate the replacement against the exact edge. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type == ON_BrepTrim::seam ||
		trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
		    trim.m_li) == repaired_seam_loops.end())
	    continue;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	int trim_offset = -1;
	for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
	    if (loop.m_ti[lti] == ti) {
		trim_offset = lti;
		break;
	    }
	}
	if (trim_offset < 0)
	    continue;
	const ON_BrepTrim *previous = loop.Trim(
	    (trim_offset + loop.TrimCount() - 1) % loop.TrimCount());
	const ON_BrepTrim *next = loop.Trim(
	    (trim_offset + 1) % loop.TrimCount());
	if (!previous || !next || previous->m_vi[1] != trim.m_vi[0] ||
		trim.m_vi[1] != next->m_vi[0])
	    continue;
	const bool has_start_gap = trim.PointAtStart().DistanceTo(
	    previous->PointAtEnd()) > ON_ZERO_TOLERANCE;
	const bool has_end_gap = trim.PointAtEnd().DistanceTo(
	    next->PointAtStart()) > ON_ZERO_TOLERANCE;
	if (trim.m_iso != ON_Surface::not_iso && !has_start_gap && !has_end_gap)
	    continue;
	brlcad::PullbackContext context;
	double uv_parameters[3][2];
	bool pulled = true;
	for (int sample = 0; sample < 3; ++sample) {
	    const double fraction = 0.125 + 0.375 * sample;
	    const ON_3dPoint target = edge.PointAt(edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(surface->Domain(0).Mid(), surface->Domain(1).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
		    LocalUnits::tolerance) || distance > LocalUnits::tolerance) {
		pulled = false;
		break;
	    }
	    ON_3dPoint refined_uv(uv.x, uv.y, 0.0);
	    double refined_distance = DBL_MAX;
	    const double numerical_tolerance = std::max(1.0e-11,
		std::min(1.0e-8, 1.0e-8 * LocalUnits::tolerance));
	    if (!refine_surface_pullback_seeded(surface, target,
		    numerical_tolerance, refined_uv, &refined_distance))
		refined_uv.Set(uv.x, uv.y, 0.0);
	    uv_parameters[sample][0] = refined_uv.x;
	    uv_parameters[sample][1] = refined_uv.y;
	}
	if (!pulled)
	    continue;

	ON_Curve *best = NULL;
	double best_score = DBL_MAX;
	std::string candidate_failures;
	for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    const double fixed_period = fixed_domain.Length();
	    double coordinates[3] = {uv_parameters[0][fixed_direction],
		uv_parameters[1][fixed_direction], uv_parameters[2][fixed_direction]};
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE) {
		for (int sample = 1; sample < 3; ++sample)
		    coordinates[sample] += round((coordinates[0] - coordinates[sample]) /
			fixed_period) * fixed_period;
	    }
	    const double minimum = std::min(coordinates[0],
		std::min(coordinates[1], coordinates[2]));
	    const double maximum = std::max(coordinates[0],
		std::max(coordinates[1], coordinates[2]));
	    if (maximum - minimum > std::max(LocalUnits::tolerance,
		    1.0e-7 * fixed_period))
		continue;
	    double fixed_parameter = (coordinates[0] + coordinates[1] +
		coordinates[2]) / 3.0;
	    /* When both topologically adjacent trims meet this edge at the same
	     * constant parameter, use that exact p-space coordinate.  The dense
	     * edge validation below still proves it represents the 3D edge, while
	     * exact reuse prevents a numerical closest-point residue from opening
	     * the loop. */
	    double adjacent_fixed[2] = {previous->PointAtEnd()[fixed_direction],
		next->PointAtStart()[fixed_direction]};
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE)
		adjacent_fixed[1] += round((adjacent_fixed[0] - adjacent_fixed[1]) /
		    fixed_period) * fixed_period;
	    if (fabs(adjacent_fixed[0] - adjacent_fixed[1]) <= ON_ZERO_TOLERANCE)
		fixed_parameter = adjacent_fixed[0];
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE) {
		while (fixed_parameter < fixed_domain.Min()) fixed_parameter += fixed_period;
		while (fixed_parameter > fixed_domain.Max()) fixed_parameter -= fixed_period;
	    }
	    const ON_3dPoint desired_start = previous->PointAtEnd();
	    const ON_3dPoint desired_end = next->PointAtStart();
	    const bool exact_adjacent_coordinates =
		fabs(desired_start[fixed_direction] - fixed_parameter) <= ON_ZERO_TOLERANCE &&
		fabs(desired_end[fixed_direction] - fixed_parameter) <= ON_ZERO_TOLERANCE;
	    double original_score = 0.0;
	    std::string failure;
	    ON_Curve *candidate = NULL;
	    for (int resolution = 256; !candidate &&
		    resolution <= kDenseValidationSegments;
		    resolution *= 2) {
		candidate = make_candidate(trim, edge, surface, fixed_direction,
		    fixed_parameter, LocalUnits::tolerance,
		    exact_adjacent_coordinates ? &desired_start : NULL,
		    exact_adjacent_coordinates ? &desired_end : NULL,
		    resolution, &original_score, &failure);
	    }
	    if (!candidate) {
		candidate_failures += " direction " + std::to_string(fixed_direction) +
		    "=" + failure;
		continue;
	    }

	    const int varying_direction = 1 - fixed_direction;
	    const double varying_period = surface->Domain(varying_direction).Length();
	    const bool varying_closed = surface->IsClosed(varying_direction) &&
		varying_period > ON_ZERO_TOLERANCE;
	    int best_period_shift = 0;
	    double candidate_score = DBL_MAX;
	    for (int period_shift = varying_closed ? -2 : 0;
		 period_shift <= (varying_closed ? 2 : 0); ++period_shift) {
		ON_3dPoint start = candidate->PointAtStart();
		ON_3dPoint end = candidate->PointAtEnd();
		start[varying_direction] += period_shift * varying_period;
		end[varying_direction] += period_shift * varying_period;
		const double closure = start.DistanceTo(previous->PointAtEnd()) +
		    end.DistanceTo(next->PointAtStart());
		if (closure < candidate_score) {
		    candidate_score = closure;
		    best_period_shift = period_shift;
		}
	    }
	    if (best_period_shift != 0) {
		ON_Xform shift(ON_Xform::IdentityTransformation);
		shift.m_xform[varying_direction][3] =
		    best_period_shift * varying_period;
		if (!candidate->Transform(shift) || !candidate->IsValid()) {
		    delete candidate;
		    continue;
		}
	    }
	    if (candidate_score >= best_score) {
		delete candidate;
		continue;
	    }
	    delete best;
	    best = candidate;
	    best_score = candidate_score;
	}
	if (!best) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact adjacent isoparametric regeneration rejected for trim "
		    << ti << "/STEP edge " << edge.m_edge_user.i << " in loop "
		    << trim.m_li << ": " << candidate_failures << std::endl;
	    continue;
	}
	const int c2_index = brep->AddTrimCurve(best);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete best;
	    continue;
	}
	brep->SetTrimIsoFlags(trim);
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated an isoparametric pcurve from the exact edge");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": regenerated "
		"isoparametric trim " << ti << " from exact STEP edge "
		<< edge.m_edge_user.i << std::endl;
    }

    for (std::vector<int>::const_iterator repaired_loop = repaired_seam_loops.begin();
	 repaired_loop != repaired_seam_loops.end(); ++repaired_loop) {
	if (*repaired_loop < 0 || *repaired_loop >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*repaired_loop];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim || trim->m_type != ON_BrepTrim::seam)
		continue;
	    int fixed_direction = -1;
	    if (trim->m_iso == ON_Surface::W_iso || trim->m_iso == ON_Surface::E_iso)
		fixed_direction = 0;
	    else if (trim->m_iso == ON_Surface::S_iso || trim->m_iso == ON_Surface::N_iso)
		fixed_direction = 1;
	    if (fixed_direction < 0)
		continue;
	    const int varying_direction = 1 - fixed_direction;
	    const ON_Interval varying_domain = surface->Domain(varying_direction);
	    const double period = varying_domain.Length();
	    if (!surface->IsClosed(varying_direction) || !(period > ON_ZERO_TOLERANCE))
		continue;
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next)
		continue;
	    int best_shift = 0;
	    double best_closure = trim->PointAtStart().DistanceTo(previous->PointAtEnd()) +
		trim->PointAtEnd().DistanceTo(next->PointAtStart());
	    for (int shift = -2; shift <= 2; ++shift) {
		ON_3dPoint start = trim->PointAtStart();
		ON_3dPoint end = trim->PointAtEnd();
		start[varying_direction] += shift * period;
		end[varying_direction] += shift * period;
		const double closure = start.DistanceTo(previous->PointAtEnd()) +
		    end.DistanceTo(next->PointAtStart());
		if (closure < best_closure) {
		    best_closure = closure;
		    best_shift = shift;
		}
	    }
	    if (best_shift == 0)
		continue;
	    ON_Curve *shifted = trim->DuplicateCurve();
	    ON_Xform transform(ON_Xform::IdentityTransformation);
	    transform.m_xform[varying_direction][3] = best_shift * period;
	    if (!shifted || !shifted->Transform(transform) ||
		    !shifted->ChangeDimension(2) || !shifted->IsValid()) {
		delete shifted;
		continue;
	    }
	    bool exact = true;
	    const ON_Interval trim_domain = trim->Domain();
	    for (int sample = 0; sample <= 64; ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint shifted_uv = shifted->PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint shifted_lift = surface->PointAt(
		    shifted_uv.x, shifted_uv.y);
		if (!original_lift.IsValid() || !shifted_lift.IsValid() ||
			original_lift.DistanceTo(shifted_lift) > ON_ZERO_TOLERANCE) {
		    exact = false;
		    break;
		}
	    }
	    if (!exact) {
		delete shifted;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(shifted);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete shifted;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact seam pcurve into its loop's periodic branch");
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || trim->m_type != ON_BrepTrim::seam || !previous || !next)
		continue;
	    const ON_3dPoint start = previous->PointAtEnd();
	    const ON_3dPoint end = next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    ON_Curve *candidate = trim->DuplicateCurve();
	    const ON_Interval domain = trim->Domain();
	    bool candidate_valid = candidate && candidate->SetStartPoint(start) &&
		candidate->SetEndPoint(end) && candidate->ChangeDimension(2) &&
		candidate->IsValid() &&
		surface->IsIsoparametric(*candidate, &domain) == trim->m_iso;
	    if (!candidate_valid) {
		delete candidate;
		candidate = new ON_LineCurve(start, end);
		candidate_valid = candidate->ChangeDimension(2) &&
		    candidate->SetDomain(domain.Min(), domain.Max()) &&
		    candidate->IsValid() &&
		    surface->IsIsoparametric(*candidate, &domain) == trim->m_iso;
	    }
	    if (!candidate_valid) {
		delete candidate;
		continue;
	    }
	    const ON_BrepEdge *edge = trim->Edge();
	    ON_NurbsCurve edge_nurbs;
	    if (!edge || !edge->GetNurbForm(edge_nurbs)) {
		delete candidate;
		continue;
	    }
	    bool valid = true;
	    for (int sample = 0; sample <= 512; ++sample) {
		const double parameter = domain.ParameterAt(
		    static_cast<double>(sample) / 512.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint candidate_lift = surface->PointAt(
		    candidate_uv.x, candidate_uv.y);
		double edge_parameter = 0.0;
		if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			original_lift.DistanceTo(candidate_lift) > LocalUnits::tolerance ||
			!ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
			    candidate_lift) || candidate_lift.DistanceTo(
				edge_nurbs.PointAt(edge_parameter)) > LocalUnits::tolerance) {
		    valid = false;
		    break;
		}
	    }
	    if (!valid) {
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched a regenerated seam to exact adjacent pcurve endpoints");
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || trim->m_type == ON_BrepTrim::seam || !previous || !next ||
		    previous->m_vi[1] != trim->m_vi[0] ||
		    trim->m_vi[1] != next->m_vi[0])
		continue;
	    /* A singular neighbor's periodic coordinate is arbitrary at the pole.
	     * Do not bend a validated ordinary edge to that value.  Preserve the
	     * ordinary edge's limiting endpoint here; the subsequent adjacent-trim
	     * pass moves only the undefined coordinate of the singular connector
	     * onto this branch while retaining its exact boundary coordinate. */
	    const ON_3dPoint start = previous->m_type == ON_BrepTrim::singular ?
		trim->PointAtStart() : previous->PointAtEnd();
	    const ON_3dPoint end = next->m_type == ON_BrepTrim::singular ?
		trim->PointAtEnd() : next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    const ON_Interval domain = trim->Domain();
	    ON_Curve *candidate = trim->DuplicateCurve();
	    bool candidate_valid = candidate && candidate->SetStartPoint(start) &&
		candidate->SetEndPoint(end) && candidate->ChangeDimension(2) &&
		candidate->IsValid();
	    if (!candidate_valid) {
		delete candidate;
		candidate = new ON_LineCurve(start, end);
		candidate_valid = candidate->ChangeDimension(2) &&
		    candidate->SetDomain(domain.Min(), domain.Max()) && candidate->IsValid();
	    }
	    if (!candidate_valid) {
		delete candidate;
		continue;
	    }
	    const ON_BrepEdge *edge = trim->Edge();
	    ON_NurbsCurve edge_nurbs;
	    if (!edge || !edge->GetNurbForm(edge_nurbs)) {
		delete candidate;
		continue;
	    }
	    bool valid = true;
	    for (int sample = 0; sample <= 256; ++sample) {
		const double fraction = static_cast<double>(sample) / 256.0;
		const double parameter = domain.ParameterAt(fraction);
		const ON_3dPoint old_uv = trim->PointAt(parameter);
		const ON_3dPoint new_uv = candidate->PointAt(parameter);
		const ON_3dPoint old_lift = surface->PointAt(old_uv.x, old_uv.y);
		const ON_3dPoint new_lift = surface->PointAt(new_uv.x, new_uv.y);
		double edge_parameter = 0.0;
		const double lift_delta = old_lift.IsValid() && new_lift.IsValid() ?
		    old_lift.DistanceTo(new_lift) : DBL_MAX;
		const bool closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		    &edge_nurbs, new_lift);
		const ON_3dPoint corresponding_edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim->m_bRev3d ? 1.0 - fraction :
			fraction));
		double edge_distance = new_lift.IsValid() && corresponding_edge_point.IsValid() ?
		    new_lift.DistanceTo(corresponding_edge_point) : DBL_MAX;
		if (closest)
		    edge_distance = std::min(edge_distance, new_lift.DistanceTo(
			edge_nurbs.PointAt(edge_parameter)));
		if (!old_lift.IsValid() || !new_lift.IsValid() ||
			lift_delta > LocalUnits::tolerance ||
			edge_distance > LocalUnits::tolerance) {
		    valid = false;
		    break;
		}
	    }
	    if (!valid) {
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched a non-seam trim to exact adjacent endpoints");
	}
	/* A loop with ordinary trims can still pass directly from one side of a
	 * periodic seam to the complementary side at a collapsed surface pole.
	 * The two seam endpoints lift to the same topological vertex, but cannot
	 * coincide in p-space.  Insert the exact singular boundary trim that STEP
	 * leaves implicit instead of moving either seam off its boundary. */
	if (loop.TrimCount() > 2) {
	    const int original_count = loop.TrimCount();
	    for (int lti = 0; lti < original_count; ++lti) {
		const ON_BrepTrim *first = loop.Trim(lti);
		const ON_BrepTrim *second = loop.Trim((lti + 1) % original_count);
		if (!first || !second || first->m_type != ON_BrepTrim::seam ||
			second->m_type != ON_BrepTrim::seam ||
			first->m_vi[1] < 0 || first->m_vi[1] != second->m_vi[0] ||
			first->m_vi[1] >= brep->m_V.Count() ||
			first->PointAtEnd().DistanceTo(second->PointAtStart()) <=
			    ON_ZERO_TOLERANCE)
		    continue;
		int seam_direction = -1;
		if ((first->m_iso == ON_Surface::W_iso &&
		     second->m_iso == ON_Surface::E_iso) ||
		    (first->m_iso == ON_Surface::E_iso &&
		     second->m_iso == ON_Surface::W_iso))
		    seam_direction = 0;
		else if ((first->m_iso == ON_Surface::S_iso &&
			  second->m_iso == ON_Surface::N_iso) ||
			 (first->m_iso == ON_Surface::N_iso &&
			  second->m_iso == ON_Surface::S_iso))
		    seam_direction = 1;
		if (seam_direction < 0 || !surface->IsClosed(seam_direction))
		    continue;
		const int singular_direction = 1 - seam_direction;
		const ON_Interval singular_domain = surface->Domain(singular_direction);
		const ON_3dPoint first_end = first->PointAtEnd();
		const ON_3dPoint second_start = second->PointAtStart();
		const double parameter = 0.5 * (first_end[singular_direction] +
		    second_start[singular_direction]);
		const int side = fabs(parameter - singular_domain.Min()) <=
		    fabs(parameter - singular_domain.Max()) ? 0 : 1;
		const ON_Surface::ISO singular_iso = singular_direction == 0 ?
		    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		ON_3dPoint start = first_end;
		ON_3dPoint end = second_start;
		start[singular_direction] = singular_domain[side];
		end[singular_direction] = singular_domain[side];
		std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(start, end));
		if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid() ||
			surface->IsIsoparametric(*singular_curve) != singular_iso)
		    continue;
		const ON_3dPoint vertex = brep->m_V[first->m_vi[1]].point;
		bool exact = true;
		for (int sample = 0; sample <= 64; ++sample) {
		    const ON_3dPoint uv = singular_curve->PointAt(
			singular_curve->Domain().ParameterAt(
			    static_cast<double>(sample) / 64.0));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    if (!lift.IsValid() || lift.DistanceTo(vertex) >
			    LocalUnits::tolerance) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		std::vector<int> original_trims;
		original_trims.reserve(original_count);
		for (int offset = 0; offset < original_count; ++offset)
		    original_trims.push_back(loop.m_ti[offset]);
		const int c2_index = brep->AddTrimCurve(singular_curve.release());
		if (c2_index < 0)
		    continue;
		const int singular_index = brep->NewSingularTrim(
		    brep->m_V[first->m_vi[1]], loop, singular_iso,
		    c2_index).m_trim_index;
		brep->m_T[singular_index].m_tolerance[0] = LocalUnits::tolerance;
		brep->m_T[singular_index].m_tolerance[1] = LocalUnits::tolerance;
		loop.m_ti.SetCount(0);
		for (int offset = 0; offset < original_count; ++offset) {
		    loop.m_ti.Append(original_trims[offset]);
		    if (offset == lti)
			loop.m_ti.Append(singular_index);
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "inserted an exact singular trim at a periodic surface pole");
		break;
	    }
	}
	if (loop.TrimCount() == 2) {
	    ON_BrepTrim *first = loop.Trim(0);
	    ON_BrepTrim *second = loop.Trim(1);
	    int seam_direction = -1;
	    if (first && second && first->m_type == ON_BrepTrim::seam &&
		    second->m_type == ON_BrepTrim::seam &&
		    ((first->m_iso == ON_Surface::W_iso &&
		      second->m_iso == ON_Surface::E_iso) ||
		     (first->m_iso == ON_Surface::E_iso &&
		      second->m_iso == ON_Surface::W_iso)))
		seam_direction = 0;
	    else if (first && second && first->m_type == ON_BrepTrim::seam &&
		    second->m_type == ON_BrepTrim::seam &&
		    ((first->m_iso == ON_Surface::S_iso &&
		      second->m_iso == ON_Surface::N_iso) ||
		     (first->m_iso == ON_Surface::N_iso &&
		      second->m_iso == ON_Surface::S_iso)))
		seam_direction = 1;
	    const int singular_direction = 1 - seam_direction;
	    if (seam_direction >= 0 && surface->IsClosed(seam_direction) &&
		    first->m_vi[1] == second->m_vi[0] &&
		    second->m_vi[1] == first->m_vi[0]) {
		const ON_Interval singular_domain = surface->Domain(singular_direction);
		const ON_3dPoint junction_start[2] = {
		    first->PointAtEnd(), second->PointAtEnd()};
		const ON_3dPoint junction_end[2] = {
		    second->PointAtStart(), first->PointAtStart()};
		const int vertex_index[2] = {first->m_vi[1], second->m_vi[1]};
		ON_Surface::ISO singular_iso[2] = {
		    ON_Surface::not_iso, ON_Surface::not_iso};
		ON_LineCurve *singular_curve[2] = {NULL, NULL};
		bool valid = true;
		for (int junction = 0; junction < 2 && valid; ++junction) {
		    const double parameter = 0.5 *
			(junction_start[junction][singular_direction] +
			 junction_end[junction][singular_direction]);
		    const int side = fabs(parameter - singular_domain.Min()) <=
			fabs(parameter - singular_domain.Max()) ? 0 : 1;
		    const int surface_side = singular_direction == 0 ?
			(side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
		    singular_iso[junction] = singular_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    if (!surface->IsSingular(surface_side) || vertex_index[junction] < 0 ||
			    vertex_index[junction] >= brep->m_V.Count()) {
			valid = false;
			break;
		    }
		    ON_3dPoint start = junction_start[junction];
		    ON_3dPoint end = junction_end[junction];
		    start[singular_direction] = singular_domain[side];
		    end[singular_direction] = singular_domain[side];
		    singular_curve[junction] = new ON_LineCurve(start, end);
		    if (!singular_curve[junction]->ChangeDimension(2) ||
			    !singular_curve[junction]->IsValid() ||
			    surface->IsIsoparametric(*singular_curve[junction]) !=
				singular_iso[junction]) {
			valid = false;
			break;
		    }
		    const ON_3dPoint vertex = brep->m_V[vertex_index[junction]].point;
		    for (int sample = 0; sample <= 64; ++sample) {
			const ON_3dPoint uv = singular_curve[junction]->PointAt(
			    singular_curve[junction]->Domain().ParameterAt(
				static_cast<double>(sample) / 64.0));
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			if (!lift.IsValid() || lift.DistanceTo(vertex) >
				    LocalUnits::tolerance) {
			    valid = false;
			    break;
			}
		    }
		}
		if (valid) {
		    const int first_index = first->m_trim_index;
		    const int second_index = second->m_trim_index;
		    const int first_c2 = brep->AddTrimCurve(singular_curve[0]);
		    const int second_c2 = brep->AddTrimCurve(singular_curve[1]);
		    if (first_c2 >= 0 && second_c2 >= 0) {
			const int first_singular = brep->NewSingularTrim(
			    brep->m_V[vertex_index[0]], loop, singular_iso[0],
			    first_c2).m_trim_index;
			const int second_singular = brep->NewSingularTrim(
			    brep->m_V[vertex_index[1]], loop, singular_iso[1],
			    second_c2).m_trim_index;
			brep->m_T[first_singular].m_tolerance[0] = LocalUnits::tolerance;
			brep->m_T[first_singular].m_tolerance[1] = LocalUnits::tolerance;
			brep->m_T[second_singular].m_tolerance[0] = LocalUnits::tolerance;
			brep->m_T[second_singular].m_tolerance[1] = LocalUnits::tolerance;
			loop.m_ti.SetCount(0);
			loop.m_ti.Append(first_index);
			loop.m_ti.Append(first_singular);
			loop.m_ti.Append(second_index);
			loop.m_ti.Append(second_singular);
			const auto adjusted_seam = [surface](const ON_BrepTrim &trim,
			    const ON_3dPoint &start, const ON_3dPoint &end) -> ON_Curve * {
			    ON_Curve *candidate = trim.DuplicateCurve();
			    const ON_Interval domain = trim.Domain();
			    if (!candidate || !candidate->SetStartPoint(start) ||
				    !candidate->SetEndPoint(end) ||
				    !candidate->ChangeDimension(2) || !candidate->IsValid() ||
				    surface->IsIsoparametric(*candidate, &domain) != trim.m_iso) {
				delete candidate;
				candidate = new ON_LineCurve(start, end);
				if (!candidate->ChangeDimension(2) ||
					!candidate->SetDomain(domain.Min(), domain.Max()) ||
					!candidate->IsValid() ||
					surface->IsIsoparametric(*candidate, &domain) != trim.m_iso) {
				    delete candidate;
				    return NULL;
				}
			    }
			    const ON_BrepEdge *edge = trim.Edge();
			    if (!edge) {
				delete candidate;
				return NULL;
			    }
			    const ON_Interval edge_domain = edge->Domain();
			    for (int sample = 0; sample <= 256; ++sample) {
				const double fraction = static_cast<double>(sample) / 256.0;
				const double parameter = domain.ParameterAt(fraction);
				const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
				const ON_3dPoint candidate_lift = surface->PointAt(
				    candidate_uv.x, candidate_uv.y);
				const double edge_fraction = trim.m_bRev3d ?
				    1.0 - fraction : fraction;
				const ON_3dPoint edge_point = edge->PointAt(
				    edge_domain.ParameterAt(edge_fraction));
				if (!candidate_lift.IsValid() || !edge_point.IsValid() ||
					candidate_lift.DistanceTo(edge_point) >
					    LocalUnits::tolerance) {
				    delete candidate;
				    return NULL;
				}
			    }
			    return candidate;
			};
			ON_BrepTrim &installed_first = brep->m_T[first_index];
			ON_BrepTrim &installed_second = brep->m_T[second_index];
			const ON_BrepTrim &north_singular = brep->m_T[first_singular];
			const ON_BrepTrim &south_singular = brep->m_T[second_singular];
			ON_Curve *first_adjusted = adjusted_seam(installed_first,
			    south_singular.PointAtEnd(), north_singular.PointAtStart());
			ON_Curve *second_adjusted = adjusted_seam(installed_second,
			    north_singular.PointAtEnd(), south_singular.PointAtStart());
			if (first_adjusted && second_adjusted) {
			    const int first_adjusted_c2 = brep->AddTrimCurve(first_adjusted);
			    const int second_adjusted_c2 = brep->AddTrimCurve(second_adjusted);
			    if (first_adjusted_c2 >= 0 && second_adjusted_c2 >= 0) {
				brep->SetTrimCurve(installed_first, first_adjusted_c2);
				brep->SetTrimCurve(installed_second, second_adjusted_c2);
				brep->SetTrimIsoFlags(installed_first);
				brep->SetTrimIsoFlags(installed_second);
			    }
			} else {
			    delete first_adjusted;
			    delete second_adjusted;
			}
			wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			    "inserted exact singular trims at periodic surface poles");
		    }
		} else {
		    delete singular_curve[0];
		    delete singular_curve[1];
		}
	    }
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *previous = loop.Trim(lti);
	    ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    /* A VERTEX_LOOP is represented by one singular trim whose endpoints
	     * span a complete collapsed surface side.  It is cyclically adjacent
	     * to itself, but MatchTrimEnds(trim, trim) contracts that legitimate
	     * span to its midpoint and destroys both the boundary ISO and BREP
	     * validity.  There is no distinct join to repair in a one-trim loop. */
	    if (!previous || !next || previous == next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count() ||
		    previous->PointAtEnd().DistanceTo(next->PointAtStart()) <=
			ON_ZERO_TOLERANCE)
		continue;
	    const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    next_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    previous_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		continue;
	    ON_Curve *previous_original = previous->DuplicateCurve();
	    ON_Curve *next_original = next->DuplicateCurve();
	    if (!previous_original || !next_original) {
		delete previous_original;
		delete next_original;
		continue;
	    }
	    const ON_Surface::ISO previous_iso = previous->m_iso;
	    const ON_Surface::ISO next_iso = next->m_iso;
	    if (!brep->MatchTrimEnds(*previous, *next)) {
		delete previous_original;
		delete next_original;
		continue;
	    }
	    const auto matched_valid = [surface](const ON_BrepTrim &trim,
		const ON_Curve &original, ON_Surface::ISO expected_iso) {
		const ON_Interval domain = trim.Domain();
		if (trim.m_type == ON_BrepTrim::seam &&
			surface->IsIsoparametric(trim, &domain) != expected_iso)
		    return false;
		const ON_BrepEdge *edge = trim.Edge();
		ON_NurbsCurve edge_nurbs;
		if (edge && !edge->GetNurbForm(edge_nurbs))
		    return false;
		for (int sample = 0; sample <= 256; ++sample) {
		    const double parameter = domain.ParameterAt(
			static_cast<double>(sample) / 256.0);
		    const ON_3dPoint old_uv = original.PointAt(parameter);
		    const ON_3dPoint new_uv = trim.PointAt(parameter);
		    const ON_3dPoint old_lift = surface->PointAt(old_uv.x, old_uv.y);
		    const ON_3dPoint new_lift = surface->PointAt(new_uv.x, new_uv.y);
		    if (!old_lift.IsValid() || !new_lift.IsValid() ||
			    old_lift.DistanceTo(new_lift) > LocalUnits::tolerance)
			return false;
		    if (edge) {
			double edge_parameter = 0.0;
			if (!ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
				new_lift) || new_lift.DistanceTo(
				    edge_nurbs.PointAt(edge_parameter)) > LocalUnits::tolerance)
			    return false;
		    }
		}
		return true;
	    };
	    const bool valid = previous->PointAtEnd().DistanceTo(next->PointAtStart()) <=
		ON_ZERO_TOLERANCE && matched_valid(*previous, *previous_original,
		    previous_iso) && matched_valid(*next, *next_original, next_iso);
	    if (!valid) {
		const int previous_c2 = brep->AddTrimCurve(previous_original);
		const int next_c2 = brep->AddTrimCurve(next_original);
		if (previous_c2 >= 0)
		    brep->SetTrimCurve(*previous, previous_c2);
		else
		    delete previous_original;
		if (next_c2 >= 0)
		    brep->SetTrimCurve(*next, next_c2);
		else
		    delete next_original;
		previous->m_iso = previous_iso;
		next->m_iso = next_iso;
		continue;
	    }
	    delete previous_original;
	    delete next_original;
	    brep->SetTrimIsoFlags(*previous);
	    brep->SetTrimIsoFlags(*next);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched exact loop endpoints within model tolerance");
	}
    }
}


void
repair_paired_seam_boundaries(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> *aligned_surface_loops)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const auto complement = [](ON_Surface::ISO iso) {
	switch (iso) {
	    case ON_Surface::W_iso: return ON_Surface::E_iso;
	    case ON_Surface::E_iso: return ON_Surface::W_iso;
	    case ON_Surface::S_iso: return ON_Surface::N_iso;
	    case ON_Surface::N_iso: return ON_Surface::S_iso;
	    default: return ON_Surface::not_iso;
	}
    };
    const auto project_to = [brep](ON_BrepTrim &trim, const ON_Surface *surface,
	const ON_BrepEdge &edge, ON_Surface::ISO target, double tolerance,
	bool aligned_surface_loop) {
	int direction = -1;
	double boundary = ON_UNSET_VALUE;
	if (target == ON_Surface::W_iso || target == ON_Surface::E_iso) {
	    direction = 0;
	    boundary = surface->Domain(0)[target == ON_Surface::E_iso ? 1 : 0];
	} else if (target == ON_Surface::S_iso || target == ON_Surface::N_iso) {
	    direction = 1;
	    boundary = surface->Domain(1)[target == ON_Surface::N_iso ? 1 : 0];
	}
	if (direction < 0 || !surface->IsClosed(direction))
	    return false;
	ON_Curve *projected = trim.DuplicateCurve();
	if (!projected)
	    return false;
	ON_Xform projection(ON_Xform::IdentityTransformation);
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[direction][column] = 0.0;
	projection.m_xform[direction][3] = boundary;
	if (!projected->Transform(projection) || !projected->ChangeDimension(2) ||
		!projected->IsValid()) {
	    delete projected;
	    return false;
	}
	const ON_Interval domain = trim.Domain();
	ON_Surface::ISO derived = surface->IsIsoparametric(*projected, &domain);
	if (derived != target) {
	    ON_LineCurve *line = new ON_LineCurve(projected->PointAtStart(),
		projected->PointAtEnd());
	    if (line->ChangeDimension(2) && line->SetDomain(domain.Min(), domain.Max()) &&
		    line->IsValid()) {
		delete projected;
		projected = line;
		derived = surface->IsIsoparametric(*projected, &domain);
	    } else {
		delete line;
	    }
	}
	if (derived != target) {
	    delete projected;
	    return false;
	}
	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs)) {
	    delete projected;
	    return false;
	}
	const int samples = std::min(kDenseValidationSegments,
	    std::max(64, trim.SpanCount() * 8));
	for (int sample = 0; sample <= samples; ++sample) {
	    const double fraction = static_cast<double>(sample) / samples;
	    const double parameter = domain.ParameterAt(fraction);
	    const ON_3dPoint original_uv = trim.PointAt(parameter);
	    const ON_3dPoint projected_uv = projected->PointAt(parameter);
	    const ON_3dPoint original_lift = surface->PointAt(original_uv.x, original_uv.y);
	    const ON_3dPoint projected_lift = surface->PointAt(projected_uv.x, projected_uv.y);
	    double edge_parameter = 0.0;
	    const bool have_closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		&edge_nurbs, projected_lift);
	    const double closest_distance = have_closest ? projected_lift.DistanceTo(
		edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
	    double edge_distance = closest_distance;
	    const ON_Interval edge_domain = edge_nurbs.Domain();
	    const ON_3dPoint forward = edge_nurbs.PointAt(
		edge_domain.ParameterAt(fraction));
	    const ON_3dPoint reverse = edge_nurbs.PointAt(
		edge_domain.ParameterAt(1.0 - fraction));
	    if (forward.IsValid())
		edge_distance = std::min(edge_distance,
		    projected_lift.DistanceTo(forward));
	    if (reverse.IsValid())
		edge_distance = std::min(edge_distance,
		    projected_lift.DistanceTo(reverse));
	    const double source_distance = original_lift.DistanceTo(projected_lift);
	    const bool doubly_closed = surface->IsClosed(0) && surface->IsClosed(1);
	    if (!original_lift.IsValid() || !projected_lift.IsValid() ||
		    (doubly_closed && !aligned_surface_loop ?
			(source_distance > tolerance || closest_distance > tolerance) :
			(source_distance > tolerance && edge_distance > tolerance))) {
		delete projected;
		return false;
	    }
	}
	const int c2_index = brep->AddTrimCurve(projected);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete projected;
	    return false;
	}
	brep->SetTrimIsoFlags(trim);
	return trim.m_iso == target;
    };

    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_index = edge.m_ti[0];
	const int second_index = edge.m_ti[1];
	if (first_index < 0 || first_index >= brep->m_T.Count() ||
		second_index < 0 || second_index >= brep->m_T.Count())
	    continue;
	ON_BrepTrim &first = brep->m_T[first_index];
	ON_BrepTrim &second = brep->m_T[second_index];
	if (first.m_type != ON_BrepTrim::seam || second.m_type != ON_BrepTrim::seam ||
		first.m_li < 0 || first.m_li != second.m_li ||
		first.m_li >= brep->m_L.Count() || complement(first.m_iso) == second.m_iso)
	    continue;
	const ON_BrepLoop &loop = brep->m_L[first.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	const bool aligned_surface_loop = aligned_surface_loops &&
	    std::find(aligned_surface_loops->begin(), aligned_surface_loops->end(),
		first.m_li) != aligned_surface_loops->end();
	bool repaired = false;
	const ON_Surface::ISO second_target = complement(first.m_iso);
	if (second_target != ON_Surface::not_iso)
	    repaired = project_to(second, surface, edge, second_target,
		LocalUnits::tolerance, aligned_surface_loop);
	if (!repaired) {
	    const ON_Surface::ISO first_target = complement(second.m_iso);
	    if (first_target != ON_Surface::not_iso)
		repaired = project_to(first, surface, edge, first_target,
		    LocalUnits::tolerance, aligned_surface_loop);
	}
	if (!repaired)
	    continue;
	wrapper->RecordRepair(entity_id, entity_type, "trim_iso",
	    "placed paired seam pcurves on opposite periodic boundaries");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": paired seam trims "
		<< first_index << '/' << second_index << " on opposite boundaries"
		<< std::endl;
    }
}


/* A periodic branch translation does not alter model-space geometry, but a
 * source pcurve/edge mismatch may already have established a local tolerance
 * larger than the document tolerance.  Reuse that tolerance only at the
 * asserted shared topology vertex, and require each lifted endpoint to agree
 * with the vertex using the tolerance belonging to its own trim/edge. */
double
periodic_trim_endpoint_tolerance(const ON_Brep *brep,
	const ON_BrepTrim &trim, int vertex_index)
{
    double tolerance = LocalUnits::tolerance;
    const ON_BrepEdge *edge = trim.Edge();
    if (edge && edge->m_tolerance > tolerance)
	tolerance = edge->m_tolerance;
    for (int axis = 0; axis < 2; ++axis)
	if (trim.m_tolerance[axis] > tolerance)
	    tolerance = trim.m_tolerance[axis];
    if (brep && vertex_index >= 0 && vertex_index < brep->m_V.Count() &&
	    brep->m_V[vertex_index].m_tolerance > tolerance)
	tolerance = brep->m_V[vertex_index].m_tolerance;
    return tolerance;
}


bool
periodic_trim_join_is_valid(const ON_Brep *brep, const ON_Surface *surface,
	const ON_BrepTrim &previous, const ON_3dPoint &previous_uv,
	const ON_BrepTrim &next, const ON_3dPoint &next_uv,
	double *lift_gap = NULL, double *join_tolerance = NULL)
{
    if (lift_gap)
	*lift_gap = DBL_MAX;
    if (join_tolerance)
	*join_tolerance = LocalUnits::tolerance;
    const int vertex_index = previous.m_vi[1];
    if (!brep || !surface || vertex_index < 0 ||
	    vertex_index != next.m_vi[0] || vertex_index >= brep->m_V.Count() ||
	    !previous_uv.IsValid() || !next_uv.IsValid())
	return false;

    const ON_3dPoint previous_lift =
	closed_surface_point_at(surface, previous_uv);
    const ON_3dPoint next_lift = closed_surface_point_at(surface, next_uv);
    const ON_3dPoint &vertex = brep->m_V[vertex_index].point;
    if (!previous_lift.IsValid() || !next_lift.IsValid() || !vertex.IsValid())
	return false;

    const double previous_tolerance =
	periodic_trim_endpoint_tolerance(brep, previous, vertex_index);
    const double next_tolerance =
	periodic_trim_endpoint_tolerance(brep, next, vertex_index);
    const double tolerance = std::max(previous_tolerance, next_tolerance);
    const double gap = previous_lift.DistanceTo(next_lift);
    if (lift_gap)
	*lift_gap = gap;
    if (join_tolerance)
	*join_tolerance = tolerance;
    return previous_lift.DistanceTo(vertex) <= previous_tolerance &&
	next_lift.DistanceTo(vertex) <= next_tolerance &&
	gap <= tolerance;
}


void
repair_aligned_surface_loop_branches(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> &aligned_surface_loops)
{
    if (!brep || !wrapper || aligned_surface_loops.empty() ||
	    !(LocalUnits::tolerance > 0.0))
	return;

    const double lift_tolerance = std::max(1.0e-10,
	std::min(1.0e-7, LocalUnits::tolerance * 1.0e-6));
    for (std::vector<int>::const_iterator loop_index =
	    aligned_surface_loops.begin();
	 loop_index != aligned_surface_loops.end(); ++loop_index) {
	if (*loop_index < 0 || *loop_index >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*loop_index];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	const double period[2] = {
	    surface->IsClosed(0) ? surface->Domain(0).Length() : 0.0,
	    surface->IsClosed(1) ? surface->Domain(1).Length() : 0.0
	};
	if (!(period[0] > ON_ZERO_TOLERANCE) &&
		!(period[1] > ON_ZERO_TOLERANCE))
	    continue;

	for (int pass = 0; pass < loop.TrimCount(); ++pass) {
	    bool changed = false;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepTrim *previous = loop.Trim(
		    (lti + loop.TrimCount() - 1) % loop.TrimCount());
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!trim || !previous || !next ||
			trim->m_type == ON_BrepTrim::seam)
		    continue;
		const ON_3dPoint original_start = trim->PointAtStart();
		const ON_3dPoint original_end = trim->PointAtEnd();
		double best_score = original_start.DistanceTo(previous->PointAtEnd()) +
		    original_end.DistanceTo(next->PointAtStart());
		int best_shift[2] = {0, 0};
		for (int shift0 = period[0] > ON_ZERO_TOLERANCE ? -1 : 0;
		     shift0 <= (period[0] > ON_ZERO_TOLERANCE ? 1 : 0); ++shift0) {
		    for (int shift1 = period[1] > ON_ZERO_TOLERANCE ? -1 : 0;
			 shift1 <= (period[1] > ON_ZERO_TOLERANCE ? 1 : 0); ++shift1) {
			if (shift0 == 0 && shift1 == 0)
			    continue;
			ON_3dPoint start = original_start;
			ON_3dPoint end = original_end;
			start.x += shift0 * period[0];
			end.x += shift0 * period[0];
			start.y += shift1 * period[1];
			end.y += shift1 * period[1];
			const double score = start.DistanceTo(previous->PointAtEnd()) +
			    end.DistanceTo(next->PointAtStart());
			if (score < best_score) {
			    best_score = score;
			    best_shift[0] = shift0;
			    best_shift[1] = shift1;
			}
		    }
		}
		if (best_shift[0] == 0 && best_shift[1] == 0)
		    continue;

		ON_Curve *candidate = trim->DuplicateCurve();
		ON_Xform translation(ON_Xform::IdentityTransformation);
		translation.m_xform[0][3] = best_shift[0] * period[0];
		translation.m_xform[1][3] = best_shift[1] * period[1];
		if (!candidate || !candidate->Transform(translation) ||
			!candidate->ChangeDimension(2) || !candidate->IsValid()) {
		    delete candidate;
		    continue;
		}

		const ON_Interval trim_domain = trim->Domain();
		bool exact = true;
		for (int sample = 0; sample <= 64; ++sample) {
		    const double parameter = trim_domain.ParameterAt(
			static_cast<double>(sample) / 64.0);
		    const ON_3dPoint original_uv = trim->PointAt(parameter);
		    const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		    const ON_3dPoint original_lift =
			closed_surface_point_at(surface, original_uv);
		    const ON_3dPoint candidate_lift =
			closed_surface_point_at(surface, candidate_uv);
		    if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			    original_lift.DistanceTo(candidate_lift) > lift_tolerance) {
			exact = false;
			break;
		    }
		}
		const ON_3dPoint candidate_start = candidate->PointAtStart();
		const ON_3dPoint candidate_end = candidate->PointAtEnd();
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		exact = exact && periodic_trim_join_is_valid(brep, surface,
		    *previous, previous_uv, *trim, candidate_start) &&
		    periodic_trim_join_is_valid(brep, surface, *trim,
			candidate_end, *next, next_uv);
		if (!exact) {
		    delete candidate;
		    continue;
		}

		const int c2_index = brep->AddTrimCurve(candidate);
		if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		    if (c2_index < 0)
			delete candidate;
		    continue;
		}
		brep->SetTrimIsoFlags(*trim);
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "unwrapped an exact non-seam pcurve after periodic surface alignment");
		changed = true;
	    }
	    if (!changed)
		break;
	}

	/* A greedy single-trim shift cannot move a contiguous branch without
	 * temporarily making its other endpoint worse.  Anchor one seam and solve
	 * the remaining whole-period choices for the complete cyclic loop.  Other
	 * seams may move only in their varying direction, preserving their fixed
	 * boundary coordinate and iso classification.  The shifts are installed
	 * only after their surface lifts and resulting joins have been verified. */
	int anchor = -1;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::seam) {
		anchor = lti;
		break;
	    }
	}
	if (anchor < 0 || loop.TrimCount() < 2)
	    continue;

	std::vector<ON_BrepTrim *> ordered_trims;
	ordered_trims.reserve(loop.TrimCount());
	bool joined_in_model_space = true;
	for (int position = 0; position < loop.TrimCount(); ++position) {
	    ON_BrepTrim *trim = loop.Trim((anchor + position) % loop.TrimCount());
	    ON_BrepTrim *previous = loop.Trim(
		(anchor + position + loop.TrimCount() - 1) % loop.TrimCount());
	    if (!trim || !previous) {
		joined_in_model_space = false;
		break;
	    }
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint current_uv = trim->PointAtStart();
	    double lift_gap = DBL_MAX;
	    double join_tolerance = LocalUnits::tolerance;
	    if (!periodic_trim_join_is_valid(brep, surface, *previous,
		    previous_uv, *trim, current_uv, &lift_gap,
		    &join_tolerance)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": aligned loop "
			<< *loop_index << " chain solve skipped at join " << position
			<< " lift=" << lift_gap << " tolerance=" << join_tolerance
			<< std::endl;
		joined_in_model_space = false;
		break;
	    }
	    ordered_trims.push_back(trim);
	}
	if (!joined_in_model_space)
	    continue;

	struct PeriodicBranchState {
	    int shift[2];
	    ON_3dPoint start;
	    ON_3dPoint end;
	};
	std::vector<std::vector<PeriodicBranchState> > states(ordered_trims.size());
	for (size_t position = 0; position < ordered_trims.size(); ++position) {
	    ON_BrepTrim *trim = ordered_trims[position];
	    int seam_fixed_direction = -1;
	    if (trim->m_type == ON_BrepTrim::seam) {
		if (trim->m_iso == ON_Surface::W_iso || trim->m_iso == ON_Surface::E_iso)
		    seam_fixed_direction = 0;
		else if (trim->m_iso == ON_Surface::S_iso || trim->m_iso == ON_Surface::N_iso)
		    seam_fixed_direction = 1;
	    }
	    const bool fixed0 = position == 0 || !(period[0] > ON_ZERO_TOLERANCE) ||
		(trim->m_type == ON_BrepTrim::seam && seam_fixed_direction != 1);
	    const bool fixed1 = position == 0 || !(period[1] > ON_ZERO_TOLERANCE) ||
		(trim->m_type == ON_BrepTrim::seam && seam_fixed_direction != 0);
	    const int minimum_shift0 = fixed0 ? 0 : -1;
	    const int maximum_shift0 = fixed0 ? 0 : 1;
	    const int minimum_shift1 = fixed1 ? 0 : -1;
	    const int maximum_shift1 = fixed1 ? 0 : 1;
	    for (int shift0 = minimum_shift0; shift0 <= maximum_shift0; ++shift0) {
		for (int shift1 = minimum_shift1; shift1 <= maximum_shift1; ++shift1) {
		    PeriodicBranchState state;
		    state.shift[0] = shift0;
		    state.shift[1] = shift1;
		    state.start = trim->PointAtStart();
		    state.end = trim->PointAtEnd();
		    state.start.x += shift0 * period[0];
		    state.end.x += shift0 * period[0];
		    state.start.y += shift1 * period[1];
		    state.end.y += shift1 * period[1];
		    states[position].push_back(state);
		}
	    }
	}

	const size_t position_count = ordered_trims.size();
	std::vector<std::vector<double> > costs(position_count);
	std::vector<std::vector<int> > predecessors(position_count);
	for (size_t position = 0; position < position_count; ++position) {
	    costs[position].assign(states[position].size(), DBL_MAX);
	    predecessors[position].assign(states[position].size(), -1);
	}
	costs[0][0] = 0.0;
	for (size_t position = 1; position < position_count; ++position) {
	    for (size_t current = 0; current < states[position].size(); ++current) {
		for (size_t previous = 0; previous < states[position - 1].size(); ++previous) {
		    if (costs[position - 1][previous] >= DBL_MAX)
			continue;
		    const double candidate_cost = costs[position - 1][previous] +
			states[position - 1][previous].end.DistanceTo(
			    states[position][current].start);
		    if (candidate_cost < costs[position][current]) {
			costs[position][current] = candidate_cost;
			predecessors[position][current] = static_cast<int>(previous);
		    }
		}
	    }
	}

	double original_cost = 0.0;
	for (size_t position = 0; position < position_count; ++position) {
	    const size_t next = (position + 1) % position_count;
	    original_cost += ordered_trims[position]->PointAtEnd().DistanceTo(
		ordered_trims[next]->PointAtStart());
	}
	double best_cost = DBL_MAX;
	int best_last = -1;
	for (size_t state = 0; state < states[position_count - 1].size(); ++state) {
	    if (costs[position_count - 1][state] >= DBL_MAX)
		continue;
	    const double cyclic_cost = costs[position_count - 1][state] +
		states[position_count - 1][state].end.DistanceTo(states[0][0].start);
	    if (cyclic_cost < best_cost) {
		best_cost = cyclic_cost;
		best_last = static_cast<int>(state);
	    }
	}
	const double improvement_floor = std::max(ON_ZERO_TOLERANCE,
	    original_cost * 1.0e-12);
	if (best_last < 0 || !(best_cost + improvement_floor < original_cost)) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": aligned loop "
		    << *loop_index << " chain solve retained cost " << original_cost
		    << " (best " << best_cost << ")" << std::endl;
	    continue;
	}

	std::vector<int> selected(position_count, 0);
	selected[position_count - 1] = best_last;
	bool complete_path = true;
	for (size_t reverse_position = position_count - 1;
		reverse_position > 0; --reverse_position) {
	    selected[reverse_position - 1] =
		predecessors[reverse_position][selected[reverse_position]];
	    if (selected[reverse_position - 1] < 0) {
		complete_path = false;
		break;
	    }
	}
	if (!complete_path)
	    continue;

	struct ShiftedTrimCurve {
	    ON_BrepTrim *trim;
	    ON_Curve *curve;
	};
	std::vector<ShiftedTrimCurve> candidates;
	bool exact = true;
	for (size_t position = 0; position < position_count; ++position) {
	    const PeriodicBranchState &state =
		states[position][selected[position]];
	    if (state.shift[0] == 0 && state.shift[1] == 0)
		continue;
	    ON_BrepTrim *trim = ordered_trims[position];
	    ON_Curve *candidate = trim->DuplicateCurve();
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[0][3] = state.shift[0] * period[0];
	    translation.m_xform[1][3] = state.shift[1] * period[1];
	    if (!candidate || !candidate->Transform(translation) ||
		    !candidate->ChangeDimension(2) || !candidate->IsValid()) {
		delete candidate;
		exact = false;
		break;
	    }
	    const ON_Interval trim_domain = trim->Domain();
	    for (int sample = 0; sample <= 64; ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		const ON_3dPoint original_lift =
		    closed_surface_point_at(surface, original_uv);
		const ON_3dPoint candidate_lift =
		    closed_surface_point_at(surface, candidate_uv);
		if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			original_lift.DistanceTo(candidate_lift) > lift_tolerance) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": aligned loop " << *loop_index
			    << " chain shift rejected for trim " << trim->m_trim_index
			    << " at sample " << sample << " lift="
			    << original_lift.DistanceTo(candidate_lift)
			    << " tolerance=" << lift_tolerance << std::endl;
		    exact = false;
		    break;
		}
	    }
	    if (!exact) {
		delete candidate;
		break;
	    }
	    ShiftedTrimCurve shifted = {trim, candidate};
	    candidates.push_back(shifted);
	}

	for (size_t position = 0; exact && position < position_count; ++position) {
	    const size_t next = (position + 1) % position_count;
	    const ON_3dPoint end_uv =
		states[position][selected[position]].end;
	    const ON_3dPoint start_uv = states[next][selected[next]].start;
	    exact = periodic_trim_join_is_valid(brep, surface,
		*ordered_trims[position], end_uv, *ordered_trims[next], start_uv);
	}
	if (!exact || candidates.empty()) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
		delete candidates[candidate].curve;
	    continue;
	}

	std::vector<int> curve_indices(candidates.size(), -1);
	bool added = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    curve_indices[candidate] = brep->AddTrimCurve(candidates[candidate].curve);
	    if (curve_indices[candidate] < 0) {
		delete candidates[candidate].curve;
		candidates[candidate].curve = NULL;
		added = false;
		break;
	    }
	}
	if (!added) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
		if (curve_indices[candidate] < 0 && candidates[candidate].curve)
		    delete candidates[candidate].curve;
	    }
	    continue;
	}
	bool installed = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    if (!brep->SetTrimCurve(*candidates[candidate].trim,
		    curve_indices[candidate])) {
		installed = false;
		break;
	    }
	    brep->SetTrimIsoFlags(*candidates[candidate].trim);
	}
	if (installed)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact pcurve chain after periodic surface alignment");
    }
}


void
repair_bounded_seam_isos(ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, bool allow_surface_alignment,
	std::vector<int> *aligned_surface_loops_out)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;
    const double tolerance = LocalUnits::tolerance;
    std::vector<int> aligned_surface_loops;
    repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type,
	NULL, &aligned_surface_loops, allow_surface_alignment);
    std::vector<int> projected_seam_loops;
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::seam ||
		trim.m_iso == ON_Surface::W_iso || trim.m_iso == ON_Surface::E_iso ||
		trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::N_iso ||
		trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_NurbsCurve edge_nurbs;
	if (!surface || !trim.TrimCurveOf() || !edge.GetNurbForm(edge_nurbs))
	    continue;
	const ON_Interval edge_domain = edge_nurbs.Domain();
	std::vector<ON_3dPoint> uv_points;
	const ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(trim.TrimCurveOf());
	if (polyline && polyline->m_pline.Count() >= 2) {
	    uv_points.reserve(polyline->m_pline.Count());
	    for (int point = 0; point < polyline->m_pline.Count(); ++point)
		uv_points.push_back(polyline->m_pline[point]);
	} else {
	    const int span_count = trim.SpanCount();
	    if (span_count <= 0 || span_count > kMaximumPcurveSpans)
		continue;
	    std::vector<double> spans(span_count + 1);
	    if (!trim.GetSpanVector(&spans[0]))
		continue;
	    uv_points.reserve(span_count * 4 + 1);
	    for (int span = 0; span < span_count; ++span) {
		for (int sub = 0; sub < 4; ++sub) {
		    const double fraction = static_cast<double>(sub) / 4.0;
		    uv_points.push_back(trim.PointAt(
			(1.0 - fraction) * spans[span] + fraction * spans[span + 1]));
		}
	    }
	    uv_points.push_back(trim.PointAt(spans[span_count]));
	}
	if (uv_points.size() < 2)
	    continue;

	double best_score = DBL_MAX;
	ON_Surface::ISO best_iso = ON_Surface::not_iso;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		bool valid = true;
		double score = 0.0;
		for (size_t segment = 0; valid && segment + 1 < uv_points.size(); ++segment) {
		    for (int sub = 0; sub < 3; ++sub) {
			const double fraction = static_cast<double>(sub) / 2.0;
			ON_3dPoint uv = (1.0 - fraction) * uv_points[segment] +
			    fraction * uv_points[segment + 1];
			ON_3dPoint boundary_uv = uv;
			boundary_uv[direction] = boundary;
			const ON_3dPoint original_lift = surface->PointAt(uv.x, uv.y);
			const ON_3dPoint boundary_lift = surface->PointAt(
			    boundary_uv.x, boundary_uv.y);
			if (!original_lift.IsValid() || !boundary_lift.IsValid()) {
			    valid = false;
			    break;
			}
			double edge_parameter = 0.0;
			const bool have_closest = ON_NurbsCurve_GetClosestPoint(
			    &edge_parameter, &edge_nurbs, boundary_lift);
			double edge_distance = have_closest ? boundary_lift.DistanceTo(
			    edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
			const double normalized = (static_cast<double>(segment) + fraction) /
			    static_cast<double>(uv_points.size() - 1);
			const ON_3dPoint forward = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(normalized));
			const ON_3dPoint reverse = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(1.0 - normalized));
			if (forward.IsValid())
			    edge_distance = std::min(edge_distance,
				boundary_lift.DistanceTo(forward));
			if (reverse.IsValid())
			    edge_distance = std::min(edge_distance,
				boundary_lift.DistanceTo(reverse));
			const double source_distance = original_lift.DistanceTo(boundary_lift);
			if (source_distance > tolerance && edge_distance > tolerance) {
			    valid = false;
			    break;
			}
			score += source_distance;
		    }
		}
		if (!valid || score >= best_score)
		    continue;
		best_score = score;
		if (direction == 0)
		    best_iso = side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
		else
		    best_iso = side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
	    }
	}
	if (best_iso == ON_Surface::not_iso) {
	    if (wrapper->Verbose()) {
		ON_BoundingBox box = trim.BoundingBox();
		std::cerr << entity_type << " #" << entity_id << ": unresolved seam trim "
		    << ti << " (loop " << trim.m_li << ", edge " << trim.m_ei
		    << ", reversed " << (trim.m_bRev3d ? "yes" : "no")
		    << ", edge uses";
		for (int use = 0; use < edge.m_ti.Count(); ++use) {
		    const int other_index = edge.m_ti[use];
		    if (other_index < 0 || other_index >= brep->m_T.Count())
			continue;
		    const ON_BrepTrim &other = brep->m_T[other_index];
		    std::cerr << ' ' << other_index << "/L" << other.m_li << '/'
			<< (other.m_bRev3d ? 'R' : 'F');
		}
		std::cerr << ", surface closed=" << (surface->IsClosed(0) ? '1' : '0')
		    << (surface->IsClosed(1) ? '1' : '0') << ", domains="
		    << surface->Domain(0).Min() << ':' << surface->Domain(0).Max() << ','
		    << surface->Domain(1).Min() << ':' << surface->Domain(1).Max()
		    << ", uv box=" << box.m_min.x << ':' << box.m_max.x << ','
		    << box.m_min.y << ':' << box.m_max.y << ')' << std::endl;
	    }
	    continue;
	}

	/* m_iso is derived state.  Merely assigning a boundary flag leaves an
	 * invalid brep because ON_Brep::IsValid() independently derives the flag
	 * from the pcurve.  Project the accepted pcurve itself onto the periodic
	 * boundary.  This affine projection preserves its parameterization and
	 * direction in the varying coordinate. */
	/* Duplicate the trim proxy, rather than its underlying C2 curve.  This
	 * preserves any active subdomain and reversal as ordinary curve geometry,
	 * allowing SetTrimCurve() to install it without protected proxy state. */
	ON_Curve *projected = trim.DuplicateCurve();
	if (!projected)
	    continue;
	int direction = -1;
	double boundary = ON_UNSET_VALUE;
	if (best_iso == ON_Surface::W_iso || best_iso == ON_Surface::E_iso) {
	    direction = 0;
	    boundary = surface->Domain(0)[best_iso == ON_Surface::E_iso ? 1 : 0];
	} else if (best_iso == ON_Surface::S_iso || best_iso == ON_Surface::N_iso) {
	    direction = 1;
	    boundary = surface->Domain(1)[best_iso == ON_Surface::N_iso ? 1 : 0];
	}
	ON_Xform projection(ON_Xform::IdentityTransformation);
	if (direction < 0) {
	    delete projected;
	    continue;
	}
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[direction][column] = 0.0;
	projection.m_xform[direction][3] = boundary;
	if (!projected->Transform(projection) || !projected->ChangeDimension(2) ||
		!projected->IsValid()) {
	    delete projected;
	    continue;
	}

	const ON_Interval trim_domain = trim.Domain();
	ON_Surface::ISO projected_iso = surface->IsIsoparametric(*projected,
	    &trim_domain);
	if (projected_iso != best_iso) {
	    /* Some imported pullbacks are geometrically on the boundary but retain
	     * a tiny backtracking wiggle in their varying coordinate.  openNURBS
	     * correctly refuses to call that an isoparametric curve.  A boundary
	     * line is an admissible regeneration only if the dense lift/edge check
	     * below proves it remains within the asserted model tolerance. */
	    ON_LineCurve *line = new ON_LineCurve(projected->PointAtStart(),
		projected->PointAtEnd());
	    if (line->ChangeDimension(2) && line->SetDomain(trim_domain.Min(),
		    trim_domain.Max()) && line->IsValid()) {
		delete projected;
		projected = line;
		projected_iso = surface->IsIsoparametric(*projected, &trim_domain);
	    } else {
		delete line;
	    }
	}
	if (projected_iso != best_iso) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": seam trim " << ti
		    << " projection rejected: derived iso "
		    << static_cast<int>(projected_iso) << " != boundary "
		    << static_cast<int>(best_iso) << std::endl;
	    delete projected;
	    continue;
	}
	const int validation_samples = std::min(kDenseValidationSegments,
	    std::max(64, trim.SpanCount() * 8));
	bool projected_valid = true;
	double rejected_fraction = 0.0;
	    double rejected_lift_distance = 0.0;
	double rejected_edge_distance = 0.0;
	for (int sample = 0; sample <= validation_samples; ++sample) {
	    const double fraction = static_cast<double>(sample) / validation_samples;
	    const double trim_parameter = trim_domain.ParameterAt(fraction);
	    const ON_3dPoint original_uv = trim.PointAt(trim_parameter);
	    const ON_3dPoint projected_uv = projected->PointAt(trim_parameter);
	    const ON_3dPoint original_lift = surface->PointAt(original_uv.x, original_uv.y);
	    const ON_3dPoint projected_lift = surface->PointAt(projected_uv.x, projected_uv.y);
	    double edge_parameter = 0.0;
	    rejected_lift_distance = original_lift.DistanceTo(projected_lift);
	    bool closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		&edge_nurbs, projected_lift);
	    rejected_edge_distance = closest ? projected_lift.DistanceTo(
		edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
	    const ON_3dPoint forward = edge_nurbs.PointAt(
		edge_domain.ParameterAt(fraction));
	    const ON_3dPoint reverse = edge_nurbs.PointAt(
		edge_domain.ParameterAt(1.0 - fraction));
	    if (forward.IsValid()) {
		rejected_edge_distance = std::min(rejected_edge_distance,
		    projected_lift.DistanceTo(forward));
		closest = true;
	    }
	    if (reverse.IsValid()) {
		rejected_edge_distance = std::min(rejected_edge_distance,
		    projected_lift.DistanceTo(reverse));
		closest = true;
	    }
	    if (!original_lift.IsValid() || !projected_lift.IsValid() ||
		    (rejected_lift_distance > tolerance &&
		     (!closest || rejected_edge_distance > tolerance))) {
		rejected_fraction = fraction;
		projected_valid = false;
		break;
	    }
	}
	if (!projected_valid) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": seam trim " << ti
		    << " projection rejected at normalized " << rejected_fraction
		    << ": lift delta=" << rejected_lift_distance
		    << ", edge distance=" << rejected_edge_distance
		    << ", tolerance=" << tolerance << std::endl;
	    delete projected;
	    continue;
	}

	const int c2_index = brep->AddTrimCurve(projected);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete projected;
	    continue;
	}
	brep->SetTrimIsoFlags(trim);
	if (std::find(projected_seam_loops.begin(), projected_seam_loops.end(),
		trim.m_li) == projected_seam_loops.end())
	    projected_seam_loops.push_back(trim.m_li);
	wrapper->RecordRepair(entity_id, entity_type, "trim_iso",
	    "projected a seam pcurve onto a periodic boundary within model tolerance");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": projected seam trim "
		<< ti << " onto periodic boundary " << static_cast<int>(best_iso)
		<< std::endl;
    }
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	aligned_surface_loops);
    for (std::vector<int>::const_iterator projected_loop =
	    projected_seam_loops.begin(); projected_loop != projected_seam_loops.end();
	 ++projected_loop) {
	if (*projected_loop < 0 || *projected_loop >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*projected_loop];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || !previous || !next || trim->m_type == ON_BrepTrim::seam ||
		    previous->m_type != ON_BrepTrim::seam ||
		    next->m_type != ON_BrepTrim::seam || trim->m_ei < 0 ||
		    trim->m_ei >= brep->m_E.Count())
		continue;
	    ON_BrepEdge &edge = brep->m_E[trim->m_ei];
	    if (edge.m_vi[0] < 0 || edge.m_vi[0] != edge.m_vi[1])
		continue;
	    const ON_3dPoint start = previous->PointAtEnd();
	    const ON_3dPoint end = next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    ON_LineCurve *candidate = new ON_LineCurve(start, end);
	    const ON_Interval trim_domain = trim->Domain();
	    bool valid = candidate->ChangeDimension(2) &&
		candidate->SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		candidate->IsValid();
	    const ON_Surface::ISO candidate_iso = valid ?
		surface->IsIsoparametric(*candidate, &trim_domain) :
		ON_Surface::not_iso;
	    const bool candidate_is_constant_parameter =
		candidate_iso != ON_Surface::not_iso;
	    const bool same_iso_direction =
		(candidate_is_constant_parameter &&
		 (trim->m_iso == ON_Surface::not_iso ||
		  (static_cast<int>(candidate_iso) % 2) ==
		  (static_cast<int>(trim->m_iso) % 2))) ||
		(candidate_iso == ON_Surface::not_iso &&
		 trim->m_iso == ON_Surface::not_iso);
	    valid = valid && same_iso_direction;
	    ON_NurbsCurve edge_nurbs;
	    const bool have_edge_nurbs = edge.GetNurbForm(edge_nurbs);
	    double rejected_edge_distance = 0.0;
	    double rejected_fraction = 0.0;
	    bool rejected_closest_point = false;
	    for (int sample = 0; valid && sample <= kDenseValidationSegments; ++sample) {
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = candidate->PointAt(
		    trim_domain.ParameterAt(fraction));
		const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
		double edge_parameter = 0.0;
		bool closest = false;
		if (lifted.IsValid() &&
			(sample == 0 || sample == kDenseValidationSegments) &&
			edge.m_vi[0] >= 0 && edge.m_vi[0] < brep->m_V.Count()) {
		    rejected_edge_distance = lifted.DistanceTo(
			brep->m_V[edge.m_vi[0]].point);
		    closest = true;
		} else if (have_edge_nurbs) {
		    closest = lifted.IsValid() &&
			ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lifted);
		    rejected_edge_distance = closest ?
			lifted.DistanceTo(edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
		    if (lifted.IsValid()) {
			const ON_Interval edge_domain = edge_nurbs.Domain();
			const ON_3dPoint forward = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(fraction));
			const ON_3dPoint reverse = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(1.0 - fraction));
			if (forward.IsValid()) {
			    rejected_edge_distance = std::min(rejected_edge_distance,
				lifted.DistanceTo(forward));
			    closest = true;
			}
			if (reverse.IsValid()) {
			    rejected_edge_distance = std::min(rejected_edge_distance,
				lifted.DistanceTo(reverse));
			    closest = true;
			}
		    }
		} else if (lifted.IsValid()) {
		    const ON_3dPoint original_uv = trim->PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint original_lift = surface->PointAt(
			original_uv.x, original_uv.y);
		    if (original_lift.IsValid()) {
			rejected_edge_distance = lifted.DistanceTo(original_lift);
			closest = true;
		    }
		}
		if (!closest || rejected_edge_distance > tolerance) {
		    rejected_fraction = fraction;
		    rejected_closest_point = !closest;
		    valid = false;
		}
	    }
	    if (!valid) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": projected-loop closed iso candidate rejected for trim "
			<< trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
			<< " candidate iso=" << static_cast<int>(candidate_iso)
			<< " original iso=" << static_cast<int>(trim->m_iso)
			<< " at normalized " << rejected_fraction
			<< (rejected_closest_point ? " (closest point failed)" : "")
			<< " edge distance=" << rejected_edge_distance
			<< " tolerance=" << tolerance << std::endl;
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": projected-loop closed iso candidate installation failed for trim "
			<< trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
			<< std::endl;
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		candidate_iso == ON_Surface::not_iso ?
		"regenerated a closed pcurve between exact seam boundaries" :
		"unwrapped a closed isoparametric edge between exact seam boundaries");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << (candidate_iso == ON_Surface::not_iso ?
			": regenerated closed trim " :
			": unwrapped closed isoparametric trim ")
		    << trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
		    << " between projected seam boundaries" << std::endl;
	}
    }
    /* Boundary projection can establish the exact periodic branches only
     * after the first seam-pair pass.  Run the bounded exact matcher again so
     * adjacent closed isoparametric edges inherit those proven endpoints. */
    if (!projected_seam_loops.empty())
	repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type,
	    &projected_seam_loops, NULL, allow_surface_alignment);
    if (aligned_surface_loops_out)
	*aligned_surface_loops_out = aligned_surface_loops;
}


void
repair_adjacent_trim_vertices(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    bool compact_needed = false;
    size_t verbose_rejections = 0;
    const int repair_budget = brep->m_V.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		ON_BrepTrim *current = loop.Trim(lti);
		ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!current || !next || current->m_vi[1] == next->m_vi[0] ||
			current->m_vi[1] < 0 || current->m_vi[1] >= brep->m_V.Count() ||
			next->m_vi[0] < 0 || next->m_vi[0] >= brep->m_V.Count())
		    continue;
		ON_BrepVertex &keep = brep->m_V[current->m_vi[1]];
		ON_BrepVertex &remove = brep->m_V[next->m_vi[0]];
		double current_tolerance = LocalUnits::tolerance;
		double next_tolerance = LocalUnits::tolerance;
		if (!wrapper->ImportOptions().exact) {
		    current_tolerance = std::max(current_tolerance,
			std::max(current->m_tolerance[0], current->m_tolerance[1]));
		    next_tolerance = std::max(next_tolerance,
			std::max(next->m_tolerance[0], next->m_tolerance[1]));
		    if (current->Edge())
			current_tolerance = std::max(current_tolerance,
			    current->Edge()->m_tolerance);
		    if (next->Edge())
			next_tolerance = std::max(next_tolerance,
			    next->Edge()->m_tolerance);
		    current_tolerance = std::max(current_tolerance,
			keep.m_tolerance);
		    next_tolerance = std::max(next_tolerance,
			remove.m_tolerance);
		}
		/* Each independently reconstructed endpoint may lie on the opposite
		 * side of the common exact surface point.  The sum of their already
		 * established local tolerances is therefore the conservative vertex
		 * separation bound; --exact deliberately retains the declared model
		 * tolerance instead. */
		const double vertex_join_tolerance =
		    wrapper->ImportOptions().exact ? LocalUnits::tolerance :
		    current_tolerance + next_tolerance;
		const double lift_join_tolerance = std::max(current_tolerance,
		    next_tolerance);
		const double vertex_distance = keep.point.DistanceTo(remove.point);
		if (keep.m_vertex_index < 0 || remove.m_vertex_index < 0 ||
			vertex_distance > vertex_join_tolerance) {
		    if (wrapper->Verbose() && verbose_rejections < 32 &&
			    (current->m_type == ON_BrepTrim::singular ||
			     next->m_type == ON_BrepTrim::singular)) {
			std::cerr << entity_type << " #" << entity_id
			    << ": adjacent singular vertex merge L" << li
			    << "/STEP" << loop.m_loop_user.i << "/T"
			    << current->m_trim_index << ':' << next->m_trim_index
			    << " rejected by vertex distance " << vertex_distance
			    << " bounds=" << current_tolerance << '/'
			    << next_tolerance << std::endl;
			++verbose_rejections;
		    }
		    continue;
		}
		const ON_3dPoint current_uv = current->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint current_lift = surface->PointAt(current_uv.x, current_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		const double current_vertex_distance = current_lift.IsValid() ?
		    current_lift.DistanceTo(keep.point) : DBL_MAX;
		const double next_vertex_distance = next_lift.IsValid() ?
		    next_lift.DistanceTo(remove.point) : DBL_MAX;
		const double lift_distance = current_lift.IsValid() &&
		    next_lift.IsValid() ? current_lift.DistanceTo(next_lift) : DBL_MAX;
		if (!current_lift.IsValid() || !next_lift.IsValid() ||
			current_lift.DistanceTo(keep.point) > current_tolerance ||
			next_lift.DistanceTo(remove.point) > next_tolerance ||
			current_lift.DistanceTo(next_lift) > lift_join_tolerance) {
		    if (wrapper->Verbose() && verbose_rejections < 32 &&
			    (current->m_type == ON_BrepTrim::singular ||
			     next->m_type == ON_BrepTrim::singular)) {
			std::cerr << entity_type << " #" << entity_id
			    << ": adjacent singular vertex merge L" << li
			    << "/STEP" << loop.m_loop_user.i << "/T"
			    << current->m_trim_index << ':' << next->m_trim_index
			    << " rejected by lift/vertex distances "
			    << current_vertex_distance << '/' << next_vertex_distance
			    << '/' << lift_distance << " bounds="
			    << current_tolerance << '/' << next_tolerance << '/'
			    << lift_join_tolerance << std::endl;
			++verbose_rejections;
		    }
		    continue;
		}
		/* Combining two loop-endpoint vertices also rewrites every incident
		 * BREP edge.  If an ordinary open edge connects this pair, the merge
		 * would turn it into a one-vertex edge without closing its 3-D curve;
		 * OpenNURBS correctly rejects that structure.  Such an edge may only
		 * be removed by the earlier dedicated collapse path, which proves its
		 * complete locus lies inside one declared-tolerance ball.  Enlarged
		 * inferred trim tolerances are not authority to collapse it here. */
		bool collapses_open_edge = false;
		int protected_edge_index = -1;
		for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
		    const ON_BrepEdge &incident = brep->m_E[ei];
		    const bool connects_pair =
			(incident.m_vi[0] == keep.m_vertex_index &&
			 incident.m_vi[1] == remove.m_vertex_index) ||
			(incident.m_vi[1] == keep.m_vertex_index &&
			 incident.m_vi[0] == remove.m_vertex_index);
		    if (connects_pair && !incident.IsClosed()) {
			collapses_open_edge = true;
			protected_edge_index = ei;
			break;
		    }
		}
		if (collapses_open_edge) {
		    if (wrapper->Verbose() && verbose_rejections < 32) {
			const ON_BrepEdge &protected_edge =
			    brep->m_E[protected_edge_index];
			std::cerr << entity_type << " #" << entity_id
			    << ": adjacent vertex merge L" << li << "/STEP"
			    << loop.m_loop_user.i << " rejected because it would "
			       "collapse open BREP edge E" << protected_edge_index
			    << "/STEP" << protected_edge.m_edge_user.i << std::endl;
			++verbose_rejections;
		    }
		    continue;
		}
		const int removed_vertex = remove.m_vertex_index;
		if (!brep->CombineCoincidentVertices(keep, remove))
		    continue;
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "merged adjacent topology vertices within model tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": merged loop "
			<< li << " vertex " << removed_vertex << " into "
			<< keep.m_vertex_index << " within measured endpoint bounds "
			<< current_tolerance << '/' << next_tolerance << std::endl;
		compact_needed = true;
		changed = true;
		break;
	    }
	}
	if (!changed)
	    break;
    }
    if (compact_needed)
	brep->Compact();
}


double
verified_regeneration_tolerance(ON_BrepTrim &trim, ON_BrepEdge &edge,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, const ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type);

double
verified_source_pcurve_tolerance(ON_BrepTrim &trim, ON_BrepEdge &edge,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, const ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type);

ON_Curve *
translated_periodic_trim_for_join(const ON_Surface *surface,
	const ON_BrepTrim &trim, const ON_3dPoint &join, bool move_start,
	std::string *failure, double bounded_lift_tolerance,
	double *measured_bounded_mismatch);


void
repair_invalid_open_pcurves(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool repair_periodic_join_discontinuities,
	std::set<int> *validated_trim_loci,
	const std::set<int> *face_source_tags)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int trim_count = brep->m_T.Count();
    for (int ti = 0; ti < trim_count; ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	const ON_Curve *source_curve = trim.TrimCurveOf();
	if (trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
		trim.m_vi[0] == trim.m_vi[1] ||
		trim.m_vi[0] >= brep->m_V.Count() ||
		trim.m_vi[1] >= brep->m_V.Count() || !source_curve ||
		trim.m_ei < 0 ||
		trim.m_ei >= brep->m_E.Count() || trim.m_li < 0 ||
		trim.m_li >= brep->m_L.Count())
	    continue;
	const ON_BrepFace *face = brep->m_L[trim.m_li].Face();
	if (face_source_tags && (!face || face_source_tags->find(
		face->m_face_user.i) == face_source_tags->end()))
	    continue;
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_NurbsCurve edge_nurbs;
	if (!surface || !edge.GetNurbForm(edge_nurbs))
	    continue;
	const bool closed_open_topology = source_curve->IsClosed();
	bool invalid_exact_endpoint = false;
	bool endpoint_exact[2] = {false, false};
	const ON_Interval trim_domain = trim.Domain();
	const ON_Interval edge_domain = edge.Domain();
	double locus_tolerance = std::max(LocalUnits::tolerance,
	    std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
	const bool measured_repair_allowed = !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	/* Dense earlier validation may have established that the source edge and
	 * surface genuinely disagree by more than the file uncertainty.  In normal
	 * safe mode, an endpoint inside that measured OpenNURBS tolerance is valid
	 * and must not be regenerated out of an already coherent periodic band.
	 * --exact retains the declared model tolerance here. */
	double endpoint_tolerance = wrapper->ImportOptions().exact ?
	    LocalUnits::tolerance : locus_tolerance;
	bool invalid_curve_locus = false;
	int failed_locus_sample = -1;
	double failed_locus_distance = 0.0;
	const auto screen_source_endpoints = [&]() {
	    invalid_exact_endpoint = false;
	    for (int end = 0; end < 2; ++end) {
		const ON_3dPoint uv = trim.PointAt(trim_domain[end]);
		const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		const ON_3dPoint edge_point = edge.PointAt(
		    edge_domain[trim.m_bRev3d ? 1 - end : end]);
		const ON_3dPoint &vertex = brep->m_V[trim.m_vi[end]].point;
		endpoint_exact[end] = uv.IsValid() && lift.IsValid() &&
		    edge_point.IsValid() && lift.DistanceTo(vertex) <=
			endpoint_tolerance && lift.DistanceTo(edge_point) <=
			endpoint_tolerance;
		invalid_exact_endpoint = invalid_exact_endpoint ||
		    !endpoint_exact[end];
	    }
	};
	const auto screen_source_locus = [&]() {
	    invalid_curve_locus = false;
	    failed_locus_sample = -1;
	    failed_locus_distance = 0.0;
	    for (int sample = 1; !invalid_curve_locus &&
		    sample < kPcurveLocusScreeningSegments; ++sample) {
		if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
		    return;
		const double fraction = static_cast<double>(sample) /
		    kPcurveLocusScreeningSegments;
		const ON_3dPoint uv = trim.PointAt(
		    trim_domain.ParameterAt(fraction));
		const ON_3dPoint lift = uv.IsValid() ?
		    closed_surface_point_at(surface, uv) : ON_3dPoint::UnsetPoint;
		const ON_3dPoint corresponding = edge.PointAt(
		    edge_domain.ParameterAt(trim.m_bRev3d ?
			1.0 - fraction : fraction));
		double distance = lift.IsValid() && corresponding.IsValid() ?
		    lift.DistanceTo(corresponding) : DBL_MAX;
		if (distance > locus_tolerance && lift.IsValid()) {
		    double closest_parameter = 0.0;
		    if (ON_NurbsCurve_GetClosestPoint(&closest_parameter,
			    &edge_nurbs, lift))
			distance = std::min(distance,
			    lift.DistanceTo(edge_nurbs.PointAt(closest_parameter)));
		}
		if (distance > locus_tolerance) {
		    invalid_curve_locus = true;
		    failed_locus_sample = sample;
		    failed_locus_distance = distance;
		}
	    }
	};
	screen_source_endpoints();
	screen_source_locus();
	/* A dense edge/surface measurement may legitimately enlarge one local
	 * OpenNURBS tolerance, but it must not hide a repairable periodic branch
	 * error.  When an internal pair of adjacent trims shares the same topology
	 * vertex yet its UV endpoints are separated by most of a closed-surface
	 * period and lift to different 3-D points beyond the declared model
	 * tolerance, force exact branch normalization before calling the loop
	 * closed.  A large parameter jump by itself is normal on a closed surface
	 * and must not cause otherwise exact adjacent curves to be regenerated.
	 * The one cyclic full-period closure of a legitimate band is deliberately
	 * exempt.  The edge-driven regeneration below is still transactional;
	 * if it cannot prove a replacement, safe mode may retain the source pcurve
	 * under the independently measured local tolerance.  This test is enabled
	 * only after surface seam and branch selection have stabilized: applying it
	 * during the initial pullback pass would regenerate valid whole-period
	 * branch choices that later topology inference is designed to resolve. */
	if (repair_periodic_join_discontinuities) {
	ON_BrepLoop &source_loop = brep->m_L[trim.m_li];
	const int source_loop_offset = source_loop.IndexOfTrim(trim);
	for (int end = 0; source_loop_offset >= 0 && end < 2; ++end) {
	    const bool cyclic_loop_join = end == 0 ? source_loop_offset == 0 :
		source_loop_offset == source_loop.TrimCount() - 1;
	    if (cyclic_loop_join)
		continue;
	    const ON_BrepTrim *adjacent = source_loop.Trim(end == 0 ?
		(source_loop_offset + source_loop.TrimCount() - 1) %
		    source_loop.TrimCount() :
		(source_loop_offset + 1) % source_loop.TrimCount());
	    const bool same_vertex = adjacent && (end == 0 ?
		adjacent->m_vi[1] == trim.m_vi[0] :
		adjacent->m_vi[0] == trim.m_vi[1]);
	    if (!same_vertex)
		continue;
	    const ON_3dPoint current_uv = trim.PointAt(trim_domain[end]);
	    const ON_3dPoint adjacent_uv = end == 0 ? adjacent->PointAtEnd() :
		adjacent->PointAtStart();
	    bool large_periodic_gap = false;
	    for (int direction = 0; direction < 2; ++direction) {
		const double period = surface->Domain(direction).Length();
		large_periodic_gap = large_periodic_gap ||
		    (surface->IsClosed(direction) &&
		     period > ON_ZERO_TOLERANCE &&
		     fabs(current_uv[direction] - adjacent_uv[direction]) >
			 0.5 * period);
	    }
	    if (!large_periodic_gap)
		continue;
	    /* Deliberately evaluate the stored parameters without periodic wrapping.
	     * Intermediate pullback may use one adjacent image of a geometrically
	     * closed, non-periodic NURBS surface, but a finished ON_Brep pcurve must
	     * lift correctly under OpenNURBS' direct surface evaluation. */
	    const ON_3dPoint current_lift = surface->PointAt(current_uv.x,
		current_uv.y);
	    const ON_3dPoint adjacent_lift = surface->PointAt(adjacent_uv.x,
		adjacent_uv.y);
	    const double join_lift_distance = current_lift.IsValid() &&
		adjacent_lift.IsValid() ? current_lift.DistanceTo(adjacent_lift) :
		DBL_MAX;
	    if (!(join_lift_distance > LocalUnits::tolerance))
		continue;
	    const ON_3dPoint &vertex = brep->m_V[trim.m_vi[end]].point;
	    invalid_exact_endpoint = true;
	    endpoint_exact[end] = false;
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": trim " << ti
		    << "/STEP edge " << edge.m_edge_user.i << " endpoint " << end
		    << " forced exact regeneration by periodic join gap "
		    << current_uv.DistanceTo(adjacent_uv) << ", lifted join error "
		    << join_lift_distance << ", and declared vertex error "
		    << (current_lift.IsValid() ? current_lift.DistanceTo(vertex) :
			DBL_MAX) << std::endl;
	}
	}
	if (invalid_curve_locus && wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": trim " << ti
		<< "/STEP edge " << edge.m_edge_user.i
		<< " supplied pcurve interior failed exact edge locus at "
		<< failed_locus_sample << '/' << kPcurveLocusScreeningSegments
		<< " distance=" << failed_locus_distance << " tolerance="
		<< locus_tolerance << std::endl;
	if (!closed_open_topology && !invalid_exact_endpoint &&
		!invalid_curve_locus) {
	    if (validated_trim_loci)
		validated_trim_loci->insert(ti);
	    continue;
	}
	std::unique_ptr<ON_Curve> original(trim.DuplicateCurve());
	if (!original)
	    continue;
	const ON_Surface::ISO original_iso = trim.m_iso;
	const ON_BrepTrim::TYPE original_type = trim.m_type;
	ON_3dPoint required_uv[2] = {ON_3dPoint::UnsetPoint,
	    ON_3dPoint::UnsetPoint};
	const ON_3dPoint *required_endpoint[2] = {NULL, NULL};
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const int loop_offset = loop.IndexOfTrim(trim);
	for (int end = 0; end < 2; ++end) {
	    const ON_BrepTrim *adjacent = NULL;
	    if (loop_offset >= 0 && loop.TrimCount() > 1)
		adjacent = end == 0 ? loop.Trim((loop_offset +
		    loop.TrimCount() - 1) % loop.TrimCount()) :
		    loop.Trim((loop_offset + 1) % loop.TrimCount());
	    const bool same_vertex = adjacent && (end == 0 ?
		adjacent->m_vi[1] == trim.m_vi[0] :
		adjacent->m_vi[0] == trim.m_vi[1]);
	    if (same_vertex) {
		required_uv[end] = end == 0 ? adjacent->PointAtEnd() :
		    adjacent->PointAtStart();
		const ON_3dPoint adjacent_lift = closed_surface_point_at(surface,
		    required_uv[end]);
		double required_tolerance = locus_tolerance;
		if (adjacent->Edge())
		    required_tolerance = std::max(required_tolerance,
			adjacent->Edge()->m_tolerance);
		required_tolerance = std::max(required_tolerance,
		    std::max(adjacent->m_tolerance[0],
			adjacent->m_tolerance[1]));
		required_tolerance = std::max(required_tolerance,
		    brep->m_V[trim.m_vi[end]].m_tolerance);
		/* An implicit periodic band may already have installed an exact
		 * seam using a densely measured source tolerance.  Preserve that
		 * shared p-space endpoint while regenerating the adjacent source
		 * edge; reverting to the smaller file uncertainty here breaks the
		 * loop after the band was proven and assembled. */
		if (required_uv[end].IsValid() && adjacent_lift.IsValid() &&
			adjacent_lift.DistanceTo(brep->m_V[trim.m_vi[end]].point) <=
			    required_tolerance)
		    required_endpoint[end] = &required_uv[end];
	    }
	    if (!required_endpoint[end] && endpoint_exact[end]) {
		required_uv[end] = trim.PointAt(trim_domain[end]);
		required_endpoint[end] = &required_uv[end];
	    }
	}
	std::string failure;
	/* This pass is entered only after the supplied pcurve has proved closed
	 * for distinct topology vertices or failed an exact endpoint test.  Start
	 * from the authoritative 3-D edge; retrying the known-invalid pcurve at
	 * 64, 128, ... 1024 segments merely repeats the same failure.  The helper
	 * still falls back to that source-driven mode if exact edge projection
	 * cannot establish a valid branch. */
	double regeneration_tolerance = LocalUnits::tolerance;
	if (measured_repair_allowed)
	    regeneration_tolerance = std::max(regeneration_tolerance,
		std::max(edge.m_tolerance,
		    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
	bool regenerated = regenerate_trim_polyline(brep, trim, surface,
	    edge_nurbs, regeneration_tolerance, &failure, NULL,
	    required_endpoint[0], required_endpoint[1], true, wrapper);
	/* Surface-seam alignment and other bounded repairs can expose a narrow
	 * source edge/surface mismatch that was not sampled when the edge was
	 * first constructed.  Do not repeatedly reject it at the declared file
	 * uncertainty.  Measure the complete exact 3-D edge against this surface
	 * at the same deterministic fractions used by final repair validation,
	 * accept only a scale-bounded result, and retry at the measured tolerance.
	 * --exact deliberately bypasses this path. */
	if (!regenerated && measured_repair_allowed) {
	    const double verified_tolerance = verified_regeneration_tolerance(
		trim, edge, surface, edge_nurbs, regeneration_tolerance, brep,
		wrapper, entity_id, entity_type);
	    if (verified_tolerance > regeneration_tolerance) {
		regeneration_tolerance = verified_tolerance;
		regenerated = regenerate_trim_polyline(brep, trim, surface,
		    edge_nurbs, regeneration_tolerance, &failure, NULL,
		    required_endpoint[0], required_endpoint[1], true, wrapper);
	    }
	}
	/* The edge-driven solver can return a pcurve on an integral-period image
	 * different from both already validated adjacent endpoints.  Its lift is
	 * still exact, so a later pairwise endpoint pass may try to bend the two
	 * ends independently and turn an isocurve into a diagonal across a
	 * periodic surface.  First move the complete regenerated pcurve by one
	 * uniform integral-period transform and require both requested endpoints
	 * literally.  validate_periodic_trim_translation() densely proves the
	 * shifted curve against the immutable edge before it is installed. */
	const auto parameter_unique_endpoint = [surface](
		const ON_3dPoint *endpoint) {
	    if (!endpoint || !surface)
		return false;
	    const ON_3dPoint native = closed_surface_native_parameter(surface,
		*endpoint);
	    /* A pole fixes the 3-D topology vertex but has no authoritative
	     * longitude in parameter space.  Requiring that arbitrary coordinate
	     * literally rejects an exact regenerated edge and can erase the
	     * singular connector topology assembled later.  Some analytic
	     * revolution surfaces do not advertise their apex through
	     * IsAtSingularity(), so also use the same differential-rank proof as
	     * regenerated endpoint validation. */
	    if (!native.IsValid())
		return true;
	    if (surface->IsAtSingularity(native.x, native.y, false))
		return false;
	    ON_3dPoint point;
	    ON_3dVector du, dv;
	    if (!surface->Ev1Der(native.x, native.y, point, du, dv))
		return true;
	    const double a = du * du;
	    const double b = du * dv;
	    const double c = dv * dv;
	    const double determinant = a * c - b * b;
	    const double numerical_floor = ON_ZERO_TOLERANCE *
		std::max(1.0, a * c);
	    return fabs(determinant) > numerical_floor;
	};
	const auto matches_required_endpoints = [surface, &required_endpoint,
		&parameter_unique_endpoint](const ON_Curve *curve) {
	    if (!curve || !surface)
		return false;
	    double parameter_tolerance = ON_ZERO_TOLERANCE *
		kNumericalToleranceScale;
	    for (int direction = 0; direction < 2; ++direction)
		parameter_tolerance = std::max(parameter_tolerance,
		    kPeriodicParameterSnapFraction * std::max(1.0,
			surface->Domain(direction).Length()));
	    const ON_Interval curve_domain = curve->Domain();
	    if (!curve_domain.IsIncreasing())
		return false;
	    for (int end = 0; end < 2; ++end) {
		if (!parameter_unique_endpoint(required_endpoint[end]))
		    continue;
		const ON_3dPoint endpoint = curve->PointAt(curve_domain[end]);
		if (!endpoint.IsValid() ||
			endpoint.DistanceTo(*required_endpoint[end]) >
			    parameter_tolerance)
		    return false;
	    }
	    return true;
	};
	const auto has_integral_period_endpoint_mismatch = [surface,
		&required_endpoint, &parameter_unique_endpoint](
		const ON_Curve *curve) {
	    if (!curve || !surface)
		return false;
	    const ON_Interval curve_domain = curve->Domain();
	    if (!curve_domain.IsIncreasing())
		return false;
	    bool have_endpoint = false;
	    bool have_nonzero_shift = false;
	    for (int end = 0; end < 2; ++end) {
		if (!parameter_unique_endpoint(required_endpoint[end]))
		    continue;
		have_endpoint = true;
		const ON_3dPoint endpoint = curve->PointAt(curve_domain[end]);
		if (!endpoint.IsValid())
		    return false;
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const double parameter_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction * std::max(1.0,
			    domain.Length()));
		    const double delta = (*required_endpoint[end])[direction] -
			endpoint[direction];
		    double shift = 0.0;
		    if (fabs(delta) > parameter_tolerance) {
			if (!surface->IsClosed(direction) ||
				!(domain.Length() > ON_ZERO_TOLERANCE))
			    return false;
			shift = round(delta / domain.Length()) * domain.Length();
			if (fabs(delta - shift) > parameter_tolerance)
			    return false;
			have_nonzero_shift = have_nonzero_shift ||
			    fabs(shift) > parameter_tolerance;
		    }
		}
	    }
	    return have_endpoint && have_nonzero_shift;
	};
	if (regenerated &&
		has_integral_period_endpoint_mismatch(trim.TrimCurveOf())) {
	    bool aligned = false;
	    std::string alignment_failure;
	    for (int anchor = 0; !aligned && anchor < 2; ++anchor) {
		if (!parameter_unique_endpoint(required_endpoint[anchor]))
		    continue;
		double measured_mismatch = 0.0;
		ON_Curve *translated = translated_periodic_trim_for_join(surface,
		    trim, *required_endpoint[anchor], anchor == 0,
		    &alignment_failure, regeneration_tolerance,
		    &measured_mismatch);
		if (!translated)
		    continue;
		if (!matches_required_endpoints(translated)) {
		    delete translated;
		    alignment_failure =
			"uniform periodic translation did not satisfy both "
			"required endpoints";
		    continue;
		}
		const int translated_c2 = brep->AddTrimCurve(translated);
		if (translated_c2 >= 0 &&
			brep->SetTrimCurve(trim, translated_c2)) {
		    brep->SetTrimIsoFlags(trim);
		    aligned = true;
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"translated a regenerated pcurve onto its required periodic branch");
		} else if (translated_c2 < 0) {
		    delete translated;
		}
	    }
	    if (!aligned) {
		regenerated = false;
		failure = alignment_failure.empty() ?
		    "regenerated pcurve did not honor its required periodic endpoints" :
		    alignment_failure;
	    }
	}
	/* Prefer a new pcurve driven by the exact 3-D edge.  Only when both that
	 * bounded regeneration and the independently measured edge/surface retry
	 * fail may safe mode retain the existing pcurve at a measured local
	 * tolerance.  This ordering is essential: applying the broader tolerance
	 * before regeneration can hide a repairable pcurve (and previously changed
	 * the already-correct NIST6 pole topology).  The fallback changes no curve,
	 * is limited to this edge/trim pair, and --exact bypasses it. */
	if (!regenerated && measured_repair_allowed) {
	    const double existing_tolerance = std::max(locus_tolerance,
		regeneration_tolerance);
	    const double verified_tolerance = verified_source_pcurve_tolerance(
		trim, edge, surface, edge_nurbs, existing_tolerance, brep, wrapper,
		entity_id, entity_type);
	    if (verified_tolerance > existing_tolerance) {
		locus_tolerance = verified_tolerance;
		endpoint_tolerance = verified_tolerance;
		screen_source_endpoints();
		screen_source_locus();
		if (!closed_open_topology && !invalid_exact_endpoint &&
			!invalid_curve_locus) {
		    if (validated_trim_loci)
			validated_trim_loci->insert(ti);
		    continue;
		}
	    }
	}
	const ON_Curve *candidate = regenerated ? trim.TrimCurveOf() : NULL;
	bool boundary_iso = trim.m_iso == ON_Surface::W_iso ||
	    trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::E_iso ||
	    trim.m_iso == ON_Surface::N_iso;
	if (regenerated && candidate && !candidate->IsClosed() &&
		original_type == ON_BrepTrim::seam && !boundary_iso) {
	    int fixed_direction = -1;
	    int fixed_side = -1;
	    double best_deviation = DBL_MAX;
	    const ON_Interval candidate_domain = candidate->Domain();
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval surface_domain = surface->Domain(direction);
		for (int side = 0; side < 2; ++side) {
		    double maximum_deviation = 0.0;
		    for (int sample = 0; sample <= 64; ++sample) {
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(
				static_cast<double>(sample) / 64.0));
			maximum_deviation = std::max(maximum_deviation,
			    fabs(uv[direction] - surface_domain[side]));
		    }
		    if (maximum_deviation < best_deviation) {
			best_deviation = maximum_deviation;
			fixed_direction = direction;
			fixed_side = side;
		    }
		}
	    }
	    std::unique_ptr<ON_Curve> projected(trim.DuplicateCurve());
	    ON_Surface::ISO target_iso = ON_Surface::not_iso;
	    if (fixed_direction == 0)
		target_iso = fixed_side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
	    else if (fixed_direction == 1)
		target_iso = fixed_side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
	    if (projected && target_iso != ON_Surface::not_iso) {
		ON_Xform projection(ON_Xform::IdentityTransformation);
		for (int column = 0; column < 4; ++column)
		    projection.m_xform[fixed_direction][column] = 0.0;
		projection.m_xform[fixed_direction][3] =
		    surface->Domain(fixed_direction)[fixed_side];
		const ON_Interval projected_domain = projected->Domain();
		bool exact = projected->Transform(projection) &&
		    projected->ChangeDimension(2) && projected->IsValid() &&
		    !projected->IsClosed() &&
		    surface->IsIsoparametric(*projected, &projected_domain) ==
			target_iso;
		for (int sample = 0; exact && sample <= kDenseValidationSegments;
			++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = projected->PointAt(
			projected_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    const ON_3dPoint corresponding = edge.PointAt(
			edge.Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - fraction : fraction));
		    double edge_parameter = 0.0;
		    double edge_distance = lift.IsValid() && corresponding.IsValid() ?
			lift.DistanceTo(corresponding) : DBL_MAX;
		if (ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lift))
		    edge_distance = std::min(edge_distance, lift.DistanceTo(
			edge_nurbs.PointAt(edge_parameter)));
		if (edge_distance > regeneration_tolerance)
		    exact = false;
		}
		if (exact) {
		    const int projected_c2 = brep->AddTrimCurve(projected.release());
		    if (projected_c2 >= 0 && brep->SetTrimCurve(trim, projected_c2)) {
			trim.m_iso = target_iso;
			candidate = trim.TrimCurveOf();
			boundary_iso = true;
		    }
		}
	    }
	}
	bool endpoints_exact = regenerated && candidate && !candidate->IsClosed();
	if (endpoints_exact) {
	    const ON_Interval candidate_domain = candidate->Domain();
	    for (int end = 0; end < 2 && endpoints_exact; ++end) {
		const ON_3dPoint uv = candidate->PointAt(candidate_domain[end]);
		const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		const ON_3dPoint edge_point = edge.PointAt(
		    edge_domain[trim.m_bRev3d ? 1 - end : end]);
		const ON_3dPoint &vertex = brep->m_V[trim.m_vi[end]].point;
		endpoints_exact = uv.IsValid() && lift.IsValid() &&
		    edge_point.IsValid() && lift.DistanceTo(vertex) <=
			regeneration_tolerance && lift.DistanceTo(edge_point) <=
			regeneration_tolerance;
	    }
	}
	if (endpoints_exact &&
		(original_type != ON_BrepTrim::seam || boundary_iso)) {
	    /* regenerate_trim_polyline() accepts only after complete dense
	     * edge/surface locus validation.  Subsequent uniform periodic
	     * translation and boundary-isocurve projection repeat that validation,
	     * so this installed locus is authoritative for the immediately
	     * following seam-occupancy pass. */
	    if (validated_trim_loci)
		validated_trim_loci->insert(ti);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		closed_open_topology ?
		"regenerated a closed pcurve for an open topology edge" :
		(original_type == ON_BrepTrim::seam ?
		 "regenerated an invalid seam pcurve from its exact edge" :
		 "regenerated an invalid open pcurve from its exact edge"));
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": regenerated "
		    << (closed_open_topology ? "closed pcurve " :
			(original_type == ON_BrepTrim::seam ?
			 "invalid seam pcurve " : "invalid open pcurve ")) << ti
		    << " for distinct topology vertices" << std::endl;
	    continue;
	}
	const bool candidate_closed = candidate && candidate->IsClosed();
	const int regenerated_iso = static_cast<int>(trim.m_iso);
	const int original_c2 = brep->AddTrimCurve(original.release());
	if (original_c2 >= 0)
	    brep->SetTrimCurve(trim, original_c2);
	trim.m_iso = original_iso;
	trim.m_type = original_type;
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": "
		<< (closed_open_topology ? "closed pcurve " :
		    (original_type == ON_BrepTrim::seam ?
		     "invalid seam pcurve " : "invalid open pcurve "))
		<< ti << "/STEP edge " << edge.m_edge_user.i
		<< " in loop " << trim.m_li
		<< " exact-edge regeneration rejected: regenerated="
		<< (regenerated ? "yes" : "no") << ", candidate="
		<< (candidate ? (candidate_closed ? "closed" : "open") : "none")
		<< ", iso=" << regenerated_iso
		<< ", tolerance=" << regeneration_tolerance
		<< (failure.empty() ? "" : ", detail=" + failure) << std::endl;
    }
}


void
repair_missing_singular_trims(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int repair_budget = brep->m_T.Count();
    int next_loop = 0;
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	int changed_loop = -1;
	for (int li = next_loop; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    ON_BrepFace *face = loop.m_fi >= 0 && loop.m_fi < brep->m_F.Count() ?
		&brep->m_F[loop.m_fi] : NULL;
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    const int original_count = loop.TrimCount();
	    if (!surface || original_count < 2)
		continue;
	    for (int lti = 0; lti < original_count; ++lti) {
		const ON_BrepTrim *first = loop.Trim(lti);
		const ON_BrepTrim *second = loop.Trim((lti + 1) % original_count);
		if (!first || !second || first->m_vi[1] < 0 ||
			first->m_vi[1] != second->m_vi[0] ||
			first->m_vi[1] >= brep->m_V.Count() ||
			first->PointAtEnd().DistanceTo(second->PointAtStart()) <=
			    ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint first_end = first->PointAtEnd();
		const ON_3dPoint second_start = second->PointAtStart();
		std::unique_ptr<ON_LineCurve> singular_curve(
		    new ON_LineCurve(first_end, second_start));
		ON_Surface::ISO singular_iso = ON_Surface::not_iso;
		if (singular_curve->ChangeDimension(2) && singular_curve->IsValid())
		    singular_iso = surface->IsIsoparametric(*singular_curve);
		if (singular_iso == ON_Surface::not_iso) {
		    /* Analytic surfaces may reject an isoparametric connector whose
		     * varying coordinate is an unwrapped periodic image far outside the
		     * native domain.  Constant-coordinate structure is independent of
		     * that branch choice and will still be densely lift-validated below. */
		    const double x_guard = std::max(ON_ZERO_TOLERANCE,
			surface->Domain(0).Length() * 1.0e-10);
		    const double y_guard = std::max(ON_ZERO_TOLERANCE,
			surface->Domain(1).Length() * 1.0e-10);
		    if (fabs(first_end.x - second_start.x) <= x_guard &&
			    fabs(first_end.y - second_start.y) > y_guard)
			singular_iso = ON_Surface::x_iso;
		    else if (fabs(first_end.y - second_start.y) <= y_guard &&
			    fabs(first_end.x - second_start.x) > x_guard)
			singular_iso = ON_Surface::y_iso;
		}
		const auto singular_side = [](ON_Surface::ISO iso) {
		    switch (iso) {
			case ON_Surface::S_iso: return 0;
			case ON_Surface::E_iso: return 1;
			case ON_Surface::N_iso: return 2;
			case ON_Surface::W_iso: return 3;
			default: return -1;
		    }
		};
		int surface_side = singular_side(singular_iso);
		const ON_Interval connector_u_domain = surface->Domain(0);
		const ON_Interval connector_v_domain = surface->Domain(1);
		const double connector_u_guard = std::max(ON_ZERO_TOLERANCE,
		    connector_u_domain.Length() * 1.0e-10);
		const double connector_v_guard = std::max(ON_ZERO_TOLERANCE,
		    connector_v_domain.Length() * 1.0e-10);
		const auto boundary_side = [](double value,
			const ON_Interval &domain, double guard) {
		    if (fabs(value - domain.Min()) <= guard)
			return 0;
		    if (fabs(value - domain.Max()) <= guard)
			return 1;
		    return -1;
		};
		const int first_u_side = boundary_side(first_end.x,
		    connector_u_domain, connector_u_guard);
		const int first_v_side = boundary_side(first_end.y,
		    connector_v_domain, connector_v_guard);
		const int second_u_side = boundary_side(second_start.x,
		    connector_u_domain, connector_u_guard);
		const int second_v_side = boundary_side(second_start.y,
		    connector_v_domain, connector_v_guard);
		const bool opposite_corners = surface_side < 0 &&
		    connector_u_domain.IsIncreasing() &&
		    connector_v_domain.IsIncreasing() &&
		    first_u_side >= 0 && first_v_side >= 0 &&
		    second_u_side >= 0 && second_v_side >= 0 &&
		    first_u_side != second_u_side &&
		    first_v_side != second_v_side;
		if (opposite_corners && wrapper->Verbose()) {
		    const ON_3dPoint &candidate_vertex =
			brep->m_V[first->m_vi[1]].point;
		    double boundary_distance[4] = {0.0, 0.0, 0.0, 0.0};
		    bool boundary_valid[4] = {true, true, true, true};
		    for (int side = 0; side < 4; ++side) {
			for (int sample = 0; sample <=
				kDenseValidationSegments; ++sample) {
			    const double fraction = static_cast<double>(sample) /
				kDenseValidationSegments;
			    ON_3dPoint uv;
			    if (side == 0)
				uv.Set(connector_u_domain.ParameterAt(fraction),
				    connector_v_domain.Min(), 0.0);
			    else if (side == 1)
				uv.Set(connector_u_domain.Max(),
				    connector_v_domain.ParameterAt(fraction), 0.0);
			    else if (side == 2)
				uv.Set(connector_u_domain.ParameterAt(fraction),
				    connector_v_domain.Max(), 0.0);
			    else
				uv.Set(connector_u_domain.Min(),
				    connector_v_domain.ParameterAt(fraction), 0.0);
			    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			    if (!lift.IsValid()) {
				boundary_valid[side] = false;
				break;
			    }
			    boundary_distance[side] = std::max(
				boundary_distance[side],
				lift.DistanceTo(candidate_vertex));
			}
		    }
		    std::cerr << entity_type << " #" << entity_id << ": loop "
			<< li << "/STEP" << loop.m_loop_user.i
			<< " non-isoparametric missing connector T"
			<< first->m_trim_index << "->T" << second->m_trim_index
			<< " uv=" << first_end.x << ':' << first_end.y << "->"
			<< second_start.x << ':' << second_start.y
			<< " surface singular(S/E/N/W)="
			<< surface->IsSingular(0) << '/'
			<< surface->IsSingular(1) << '/'
			<< surface->IsSingular(2) << '/'
			<< surface->IsSingular(3)
			<< " boundary maximum vertex distances="
			<< (boundary_valid[0] ? boundary_distance[0] : DBL_MAX)
			<< '/' << (boundary_valid[1] ? boundary_distance[1] : DBL_MAX)
			<< '/' << (boundary_valid[2] ? boundary_distance[2] : DBL_MAX)
			<< '/' << (boundary_valid[3] ? boundary_distance[3] : DBL_MAX)
			<< std::endl;
		}
		if (opposite_corners) {
		    /* A rectangular surface parameterization can represent a corner
		     * pole with two consecutive collapsed sides.  STEP then needs no
		     * geometric edge between opposite UV corners, but OpenNURBS needs
		     * both zero-length topology uses explicitly.  Test the two
		     * possible native-boundary routes and accept only one whose
		     * complete sides densely lift to the asserted vertex.
		     *
		     * Do not select a route which repeats an adjacent boundary trim:
		     * that would create overlapping CDT constraints even though the
		     * duplicate singular curves are structurally legal. */
		    struct CollapsedCornerPath {
			ON_3dPoint points[3];
			ON_Surface::ISO iso[2];
			int side[2];
			double maximum_distance;
			double tolerance;
			bool adjusted;
			bool valid;
		    };
		    CollapsedCornerPath paths[2] = {};
		    paths[0].points[0] = first_end;
		    paths[0].points[1].Set(second_start.x, first_end.y, 0.0);
		    paths[0].points[2] = second_start;
		    paths[0].iso[0] = first_v_side == 0 ?
			ON_Surface::S_iso : ON_Surface::N_iso;
		    paths[0].side[0] = first_v_side == 0 ? 0 : 2;
		    paths[0].iso[1] = second_u_side == 0 ?
			ON_Surface::W_iso : ON_Surface::E_iso;
		    paths[0].side[1] = second_u_side == 0 ? 3 : 1;
		    paths[1].points[0] = first_end;
		    paths[1].points[1].Set(first_end.x, second_start.y, 0.0);
		    paths[1].points[2] = second_start;
		    paths[1].iso[0] = first_u_side == 0 ?
			ON_Surface::W_iso : ON_Surface::E_iso;
		    paths[1].side[0] = first_u_side == 0 ? 3 : 1;
		    paths[1].iso[1] = second_v_side == 0 ?
			ON_Surface::S_iso : ON_Surface::N_iso;
		    paths[1].side[1] = second_v_side == 0 ? 0 : 2;

		    ON_BoundingBox surface_bounds;
		    const double surface_scale = surface->GetBoundingBox(
			surface_bounds, false) && surface_bounds.IsValid() ?
			surface_bounds.Diagonal().Length() : 0.0;
		    const double adjustment_limit = std::max(
			LocalUnits::tolerance, surface_scale *
			kCollapsedBoundaryMaximumRelativeMismatch);
		    const ON_3dPoint &vertex =
			brep->m_V[first->m_vi[1]].point;
		    const bool nurbs_surface = ON_NurbsSurface::Cast(surface) != NULL;
		    int valid_paths = 0;
		    int selected_path = -1;
		    for (int path_index = 0; path_index < 2; ++path_index) {
			CollapsedCornerPath &path = paths[path_index];
			path.valid = true;
			path.maximum_distance = 0.0;
			path.tolerance = LocalUnits::tolerance;
			path.adjusted = false;
			for (int segment = 0; segment < 2; ++segment) {
			    if (path.iso[segment] == first->m_iso ||
				    path.iso[segment] == second->m_iso) {
				path.valid = false;
				break;
			    }
			    if (!surface->IsSingular(path.side[segment]) &&
				    !nurbs_surface) {
				path.valid = false;
				break;
			    }
			    ON_LineCurve boundary(path.points[segment],
				path.points[segment + 1]);
			    const ON_Interval boundary_domain = boundary.Domain();
			    if (!boundary.ChangeDimension(2) || !boundary.IsValid() ||
				    surface->IsIsoparametric(boundary,
					&boundary_domain) != path.iso[segment]) {
				path.valid = false;
				break;
			    }
			    for (int sample = 0; path.valid &&
				    sample <= kDenseValidationSegments; ++sample) {
				const ON_3dPoint uv = boundary.PointAt(
				    boundary.Domain().ParameterAt(
					static_cast<double>(sample) /
					kDenseValidationSegments));
				const ON_3dPoint lift =
				    closed_surface_point_at(surface, uv);
				if (!lift.IsValid()) {
				    path.valid = false;
				    break;
				}
				path.maximum_distance = std::max(
				    path.maximum_distance, lift.DistanceTo(vertex));
			    }
			}
			if (!path.valid)
			    continue;
			if (path.maximum_distance > LocalUnits::tolerance) {
			    const double adjusted = path.maximum_distance *
				kRegenerationToleranceSafety;
			    if (wrapper->ImportOptions().exact ||
				    wrapper->ImportOptions().repair !=
					brlcad::step::RepairMode::Safe ||
				    adjusted > adjustment_limit) {
				path.valid = false;
				continue;
			    }
			    path.tolerance = adjusted;
			    path.adjusted = true;
			}
			++valid_paths;
			selected_path = path_index;
		    }
		    if (valid_paths == 1 && selected_path >= 0) {
			const CollapsedCornerPath &path = paths[selected_path];
			const int corner_vertex_index = first->m_vi[1];
			std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
			std::vector<int> original_trims;
			original_trims.reserve(original_count);
			for (int offset = 0; offset < original_count; ++offset)
			    original_trims.push_back(loop.m_ti[offset]);
			std::unique_ptr<ON_LineCurve> first_curve(
			    new ON_LineCurve(path.points[0], path.points[1]));
			std::unique_ptr<ON_LineCurve> second_curve(
			    new ON_LineCurve(path.points[1], path.points[2]));
			bool installed = first_curve->ChangeDimension(2) &&
			    second_curve->ChangeDimension(2) &&
			    first_curve->IsValid() && second_curve->IsValid();
			const int first_c2 = installed ?
			    brep->AddTrimCurve(first_curve.release()) : -1;
			const int second_c2 = installed ?
			    brep->AddTrimCurve(second_curve.release()) : -1;
			installed = first_c2 >= 0 && second_c2 >= 0;
			int first_singular = -1;
			int second_singular = -1;
			if (installed) {
			    first_singular = brep->NewSingularTrim(
				brep->m_V[corner_vertex_index], loop, path.iso[0],
				first_c2).m_trim_index;
			    second_singular = brep->NewSingularTrim(
				brep->m_V[corner_vertex_index], loop, path.iso[1],
				second_c2).m_trim_index;
			    installed = first_singular >= 0 &&
				second_singular >= 0;
			}
			if (installed) {
			    brep->m_T[first_singular].m_tolerance[0] =
				path.tolerance;
			    brep->m_T[first_singular].m_tolerance[1] =
				path.tolerance;
			    brep->m_T[second_singular].m_tolerance[0] =
				path.tolerance;
			    brep->m_T[second_singular].m_tolerance[1] =
				path.tolerance;
			    loop.m_ti.SetCount(0);
			    for (int offset = 0; offset < original_count; ++offset) {
				loop.m_ti.Append(original_trims[offset]);
				if (offset == lti) {
				    loop.m_ti.Append(first_singular);
				    loop.m_ti.Append(second_singular);
				}
			    }
			    if (path.adjusted) {
				brep->m_V[corner_vertex_index].m_tolerance = std::max(
				    brep->m_V[corner_vertex_index].m_tolerance,
				    path.tolerance);
				wrapper->RecordDiagnostic(
				    brlcad::step::DiagnosticSeverity::Warning,
				    entity_id, entity_type, "edge_loop",
				    "two source-collapsed corner boundaries exceeded "
				    "the declared tolerance; used a densely measured "
				    "OpenNURBS tolerance");
			    }
			    wrapper->RecordRepair(entity_id, entity_type,
				"edge_loop",
				path.adjusted ?
				"inserted a measured-tolerance two-side singular corner chain" :
				"inserted an exact two-side singular corner chain");
			    changed = true;
			    break;
			}
			*brep = *rollback;
			return;
		    } else if (wrapper->Verbose()) {
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << "/STEP" << loop.m_loop_user.i
			    << " rejected opposite-corner singular chain: "
			    << valid_paths << " exact boundary routes" << std::endl;
		    }
		}
		if (changed)
		    break;
		bool interior_connector_collapsed = false;
		if (surface_side < 0 && first->m_vi[1] >= 0 &&
			first->m_vi[1] < brep->m_V.Count() &&
			(singular_iso == ON_Surface::x_iso ||
			 singular_iso == ON_Surface::y_iso)) {
		    interior_connector_collapsed = true;
		    const ON_3dPoint &candidate_vertex =
			brep->m_V[first->m_vi[1]].point;
		    for (int sample = 0; interior_connector_collapsed &&
			    sample <= kDenseValidationSegments; ++sample) {
			const ON_3dPoint uv = singular_curve->PointAt(
			    singular_curve->Domain().ParameterAt(
				static_cast<double>(sample) /
				kDenseValidationSegments));
			const ON_3dPoint lift = closed_surface_point_at(surface, uv);
			interior_connector_collapsed = lift.IsValid() &&
			    lift.DistanceTo(candidate_vertex) <= LocalUnits::tolerance;
		    }
		}
		if (surface_side < 0 &&
			interior_connector_collapsed &&
			(singular_iso == ON_Surface::x_iso ||
			 singular_iso == ON_Surface::y_iso)) {
		    /* Analytic cones can place their collapsed apex isoparameter in
		     * the interior of an overly broad generated surface domain.  A
		     * singular trim is valid only on a surface boundary, so restrict a
		     * private copy of this face's surface to the occupied side of the
		     * proven collapsed isoparameter.  Trimming a surface domain is exact;
		     * all existing pcurve lifts are checked before the face is switched. */
		    const int fixed_direction = singular_iso == ON_Surface::x_iso ? 0 : 1;
		    const ON_3dPoint vertex = brep->m_V[first->m_vi[1]].point;
		    /* The two supplied endpoints already lift to the shared STEP
		     * vertex within its asserted uncertainty.  Their common fixed
		     * parameter is therefore the authoritative collapsed side; a global
		     * closest-point solve on an extended analytic cone can select a
		     * different sheet of the same infinite surface. */
		    const double collapsed_parameter = 0.5 *
			(first_end[fixed_direction] + second_start[fixed_direction]);
		    const ON_Interval old_domain = surface->Domain(fixed_direction);
		    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
			old_domain.Length() * 1.0e-10);
		    bool occupied_low = false;
		    bool occupied_high = false;
		    for (int check_lti = 0; check_lti < loop.TrimCount(); ++check_lti) {
			const ON_BrepTrim *check = loop.Trim(check_lti);
			if (!check) continue;
			const ON_Interval check_domain = check->Domain();
			/* Invalid supplied pcurves can overshoot onto the opposite
			 * analytic sheet between their exact topology endpoints.  Use
			 * only those endpoints to choose the occupied side; all complete
			 * pcurve lifts are independently checked before installation. */
			for (int sample = 0; sample < 2; ++sample) {
			    const ON_3dPoint uv = check->PointAt(check_domain.ParameterAt(
				static_cast<double>(sample)));
			    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
			    if (lift.IsValid() && lift.DistanceTo(vertex) <=
				    LocalUnits::tolerance)
				continue;
			    occupied_low = occupied_low || uv[fixed_direction] <
				collapsed_parameter - parameter_guard;
			    occupied_high = occupied_high || uv[fixed_direction] >
				collapsed_parameter + parameter_guard;
			}
		    }
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " interior collapsed isoparameter direction="
			    << fixed_direction << " parameter=" << collapsed_parameter
			    << " domain=" << old_domain.Min() << ':' << old_domain.Max()
			    << " occupied=" << (occupied_low ? 'L' : '-')
			    << (occupied_high ? 'H' : '-') << std::endl;
		    /* A horn-torus sheet can terminate at a collapsed isoparameter
		     * inside the native analytic domain.  The exact edge pullbacks on
		     * either side then meet the same STEP vertex at different, equally
		     * valid longitudes.  OpenNURBS needs both the occupied half-surface
		     * and its zero-length boundary use made explicit.
		     *
		     * Build both changes on a private BREP.  The shared sheet helper
		     * proves a unique occupied half, translates every face pcurve into
		     * that domain without changing its lift, and installs a private
		     * surface.  Retain the transaction only when the affected loop and
		     * face validate; unrelated pre-existing failures elsewhere in a
		     * large shell do not suppress this local exact repair. */
		    if (old_domain.Includes(collapsed_parameter, true) &&
			    occupied_low != occupied_high) {
			const int face_index = face->m_face_index;
			const int winding_direction = 1 - fixed_direction;
			std::unique_ptr<ON_Brep> sheet_candidate(new ON_Brep(*brep));
			bool installed = face_index >= 0 &&
			    face_index < sheet_candidate->m_F.Count() &&
			    li >= 0 && li < sheet_candidate->m_L.Count();
			if (installed) {
			    ON_BrepLoop &candidate_loop = sheet_candidate->m_L[li];
			    std::vector<int> original_trims;
			    original_trims.reserve(original_count);
			    for (int offset = 0; offset < original_count; ++offset)
				original_trims.push_back(candidate_loop.m_ti[offset]);
			    ON_3dPoint start = first_end;
			    ON_3dPoint end = second_start;
			    start[fixed_direction] = collapsed_parameter;
			    end[fixed_direction] = collapsed_parameter;
			    std::unique_ptr<ON_LineCurve> connector(
				new ON_LineCurve(start, end));
			    installed = connector->ChangeDimension(2) &&
				connector->IsValid();
			    const int c2 = installed ?
				sheet_candidate->AddTrimCurve(connector.release()) : -1;
			    installed = c2 >= 0;
			    if (installed) {
				const ON_Surface::ISO boundary_iso = fixed_direction == 0 ?
				    (occupied_high ? ON_Surface::W_iso :
					ON_Surface::E_iso) :
				    (occupied_high ? ON_Surface::S_iso :
					ON_Surface::N_iso);
				const int singular_index =
				    sheet_candidate->NewSingularTrim(
					sheet_candidate->m_V[first->m_vi[1]],
					candidate_loop, boundary_iso, c2).m_trim_index;
				installed = singular_index >= 0;
				if (installed) {
				    sheet_candidate->m_T[singular_index].m_tolerance[0] =
					LocalUnits::tolerance;
				    sheet_candidate->m_T[singular_index].m_tolerance[1] =
					LocalUnits::tolerance;
				    candidate_loop.m_ti.SetCount(0);
				    for (int offset = 0; offset < original_count; ++offset) {
					candidate_loop.m_ti.Append(original_trims[offset]);
					if (offset == lti)
					    candidate_loop.m_ti.Append(singular_index);
				    }
				}
			    }
			}
			installed = installed &&
			    restrict_periodic_face_to_interior_pole_sheet(
				sheet_candidate.get(), face_index, winding_direction,
				wrapper, entity_id, entity_type, false);
			ON_wString loop_messages;
			ON_TextLog loop_log(loop_messages);
			ON_wString face_messages;
			ON_TextLog face_log(face_messages);
			installed = installed &&
			    sheet_candidate->m_L[li].IsValid(&loop_log) &&
			    sheet_candidate->m_F[face_index].IsValid(&face_log);
			if (installed) {
			    *brep = *sheet_candidate;
			    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
				"inserted an exact singular trim at an interior periodic pole");
			    wrapper->RecordRepair(entity_id, entity_type, "face_surface",
				"restricted a degenerate periodic surface to its exact occupied pole sheet");
			    changed = true;
			    break;
			}
		    }
		    if (changed)
			break;
		    if (old_domain.Includes(collapsed_parameter, true) &&
			    occupied_low != occupied_high) {
			const int side = occupied_high ? 0 : 1;
			const ON_Interval retained = occupied_high ?
			    ON_Interval(collapsed_parameter, old_domain.Max()) :
			    ON_Interval(old_domain.Min(), collapsed_parameter);
			ON_Surface *candidate = surface->DuplicateSurface();
			bool exact_surface = candidate && retained.IsIncreasing() &&
			    candidate->Trim(fixed_direction, retained) &&
			    candidate->IsValid();
			/* A face can own several outer/inner loops that all use the same
			 * proxy surface.  Restricting that surface after checking only the
			 * loop containing this collapsed connector leaves every other loop
			 * in the old parameter domain; OpenNURBS then extrapolates their
			 * stale UVs, sometimes by model-scale distances.  Prove the private
			 * surface copy preserves every trim on the face before switching it. */
			for (int check_fli = 0; exact_surface &&
				check_fli < face->m_li.Count(); ++check_fli) {
			    const int check_loop_index = face->m_li[check_fli];
			    const ON_BrepLoop *check_loop =
				check_loop_index >= 0 &&
				check_loop_index < brep->m_L.Count() ?
				&brep->m_L[check_loop_index] : NULL;
			    if (!check_loop) {
				exact_surface = false;
				break;
			    }
			    for (int check_lti = 0; exact_surface &&
				    check_lti < check_loop->TrimCount(); ++check_lti) {
				const ON_BrepTrim *check = check_loop->Trim(check_lti);
				if (!check) {
				    exact_surface = false;
				    break;
				}
				const ON_Interval check_domain = check->Domain();
				for (int sample = 0; exact_surface && sample <= 64;
					++sample) {
				    const ON_3dPoint uv = check->PointAt(
					check_domain.ParameterAt(
					    static_cast<double>(sample) / 64.0));
				    const ON_Interval candidate_u = candidate->Domain(0);
				    const ON_Interval candidate_v = candidate->Domain(1);
				    if (!uv.IsValid() || !candidate_u.Includes(uv.x, true) ||
					    !candidate_v.Includes(uv.y, true)) {
					exact_surface = false;
					break;
				    }
				    const ON_3dPoint old_lift = surface->PointAt(uv.x, uv.y);
				    const ON_3dPoint new_lift = candidate->PointAt(uv.x, uv.y);
				    exact_surface = old_lift.IsValid() && new_lift.IsValid() &&
					old_lift.DistanceTo(new_lift) <=
					    std::max(ON_ZERO_TOLERANCE *
						kNumericalToleranceScale,
						LocalUnits::tolerance * 1.0e-8);
				}
			    }
			}
			if (exact_surface) {
				    const int surface_index = brep->AddSurface(candidate);
				    if (surface_index >= 0) {
					face->m_si = surface_index;
					face->SetProxySurface(candidate);
					surface = candidate;
				candidate = NULL;
				ON_3dPoint start = first_end;
				ON_3dPoint end = second_start;
				start[fixed_direction] = collapsed_parameter;
				end[fixed_direction] = collapsed_parameter;
				singular_curve.reset(new ON_LineCurve(start, end));
				singular_iso = fixed_direction == 0 ?
				    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
				    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
				surface_side = singular_side(singular_iso);
				wrapper->RecordRepair(entity_id, entity_type,
				    "face_surface",
				    "restricted an analytic surface to its exact collapsed apex");
			    }
			}
			if (!exact_surface && wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id << ": loop "
				<< li << " rejected exact analytic surface restriction at "
				<< collapsed_parameter << std::endl;
			delete candidate;
		    }
		}
		if (surface_side < 0) {
		    /* Some imported NURBS surfaces collapse a boundary without
		     * advertising it through IsSingular().  Project the straight
		     * connector onto the nearest exact boundary; the dense model-space
		     * vertex test below, not the missing flag, is authoritative. */
		    const int varying_direction = fabs(second_start.y - first_end.y) >
			fabs(second_start.x - first_end.x) ? 1 : 0;
		    const int fixed_direction = 1 - varying_direction;
		    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		    const double fixed_parameter = 0.5 *
			(first_end[fixed_direction] + second_start[fixed_direction]);
		    const int side = fabs(fixed_parameter - fixed_domain.Min()) <=
			fabs(fixed_parameter - fixed_domain.Max()) ? 0 : 1;
		    ON_3dPoint start = first_end;
		    ON_3dPoint end = second_start;
		    start[fixed_direction] = fixed_domain[side];
		    end[fixed_direction] = fixed_domain[side];
		    std::unique_ptr<ON_LineCurve> boundary_curve(
			new ON_LineCurve(start, end));
		    const ON_Surface::ISO boundary_iso = fixed_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    if (boundary_curve->ChangeDimension(2) && boundary_curve->IsValid() &&
			    surface->IsIsoparametric(*boundary_curve) == boundary_iso) {
			singular_curve = std::move(boundary_curve);
			singular_iso = boundary_iso;
			surface_side = singular_side(singular_iso);
		    }
		}
		if (surface_side < 0) {
		    /* A paired periodic seam can be a few parameter-space ulps away
		     * from the exact singular boundary.  Project only that established
		     * seam configuration to the boundary before testing its 3D lift. */
		    int seam_direction = -1;
		    if (first->m_type == ON_BrepTrim::seam &&
			    second->m_type == ON_BrepTrim::seam &&
			    ((first->m_iso == ON_Surface::W_iso &&
			      second->m_iso == ON_Surface::E_iso) ||
			     (first->m_iso == ON_Surface::E_iso &&
			      second->m_iso == ON_Surface::W_iso)))
			seam_direction = 0;
		    else if (first->m_type == ON_BrepTrim::seam &&
			    second->m_type == ON_BrepTrim::seam &&
			    ((first->m_iso == ON_Surface::S_iso &&
			      second->m_iso == ON_Surface::N_iso) ||
			     (first->m_iso == ON_Surface::N_iso &&
			      second->m_iso == ON_Surface::S_iso)))
			seam_direction = 1;
		    if (seam_direction < 0 || !surface->IsClosed(seam_direction))
			continue;
		    const int singular_direction = 1 - seam_direction;
		    const ON_Interval singular_domain =
			surface->Domain(singular_direction);
		    const double parameter = 0.5 *
			(first_end[singular_direction] +
			 second_start[singular_direction]);
		    const int side = fabs(parameter - singular_domain.Min()) <=
			fabs(parameter - singular_domain.Max()) ? 0 : 1;
		    surface_side = singular_direction == 0 ?
			(side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
		    if (!surface->IsSingular(surface_side))
			continue;
		    singular_iso = singular_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    ON_3dPoint start = first_end;
		    ON_3dPoint end = second_start;
		    start[singular_direction] = singular_domain[side];
		    end[singular_direction] = singular_domain[side];
		    singular_curve.reset(new ON_LineCurve(start, end));
		    if (!singular_curve->ChangeDimension(2) ||
			    !singular_curve->IsValid() ||
			    surface->IsIsoparametric(*singular_curve) != singular_iso)
			continue;
		}
		const int vertex_index = first->m_vi[1];
		const ON_3dPoint vertex = brep->m_V[vertex_index].point;
		double collapse_tolerance = LocalUnits::tolerance;
		double measured_collapse_distance = 0.0;
		double collapse_adjustment_limit = LocalUnits::tolerance;
		bool measured_tolerance_adjustment = false;
		if (!surface->IsSingular(surface_side)) {
		    /* Only a supplied NURBS boundary can be geometrically collapsed
		     * without advertising ON_Surface::IsSingular().  Applying this
		     * inference to a long analytic cylinder lets an item-scale allowance
		     * misclassify its complete circular boundary as one vertex.  Analytic
		     * surfaces must expose a real singular side (or take the proven cone
		     * restriction path above). */
		    if (!ON_NurbsSurface::Cast(surface))
			continue;
		    const int fixed_direction =
			(surface_side == 1 || surface_side == 3) ? 0 : 1;
		    const int varying_direction = 1 - fixed_direction;
		    const int side =
			(surface_side == 1 || surface_side == 2) ? 1 : 0;
		    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		    const ON_Interval varying_domain = surface->Domain(varying_direction);
		    bool collapsed_boundary = fixed_domain.IsIncreasing() &&
			varying_domain.IsIncreasing();
		    double measured_boundary_distance = 0.0;
		    for (int sample = 0; collapsed_boundary &&
			    sample <= kDenseValidationSegments; ++sample) {
			ON_3dPoint uv;
			uv[fixed_direction] = fixed_domain[side];
			uv[varying_direction] = varying_domain.ParameterAt(
			    static_cast<double>(sample) / kDenseValidationSegments);
			uv.z = 0.0;
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			collapsed_boundary = lift.IsValid();
			if (collapsed_boundary)
			    measured_boundary_distance = std::max(
				measured_boundary_distance, lift.DistanceTo(vertex));
		    }
		    if (collapsed_boundary && measured_boundary_distance >
			    LocalUnits::tolerance) {
			ON_BoundingBox surface_bounds;
			const double surface_scale = surface->GetBoundingBox(
			    surface_bounds, false) && surface_bounds.IsValid() ?
			    surface_bounds.Diagonal().Length() : 0.0;
			const double adjustment_limit = std::max(
			    LocalUnits::tolerance, surface_scale *
			    kCollapsedBoundaryMaximumRelativeMismatch);
			measured_collapse_distance = measured_boundary_distance;
			collapse_adjustment_limit = adjustment_limit;
			const double adjusted = measured_boundary_distance *
			    kRegenerationToleranceSafety;
			if (wrapper->ImportOptions().exact ||
				wrapper->ImportOptions().repair !=
				    brlcad::step::RepairMode::Safe ||
				!(adjusted <= adjustment_limit))
			    collapsed_boundary = false;
			else {
			    collapse_tolerance = adjusted;
			    measured_tolerance_adjustment = true;
			}
		    }
		    if (!collapsed_boundary)
			continue;
		}
		bool exact = true;
		for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		    const ON_3dPoint uv = singular_curve->PointAt(
			singular_curve->Domain().ParameterAt(
			    static_cast<double>(sample) / kDenseValidationSegments));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    if (!lift.IsValid() || lift.DistanceTo(vertex) >
			    collapse_tolerance) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		if (measured_tolerance_adjustment) {
		    ON_BrepVertex &adjusted_vertex = brep->m_V[vertex_index];
		    adjusted_vertex.m_tolerance = std::max(
			adjusted_vertex.m_tolerance, collapse_tolerance);
		    const int adjacent_trim_indices[2] = {
			first->m_trim_index, second->m_trim_index
		    };
		    for (int adjacent = 0; adjacent < 2; ++adjacent) {
			const int trim_index = adjacent_trim_indices[adjacent];
			if (trim_index < 0 || trim_index >= brep->m_T.Count())
			    continue;
			ON_BrepTrim &adjusted_trim = brep->m_T[trim_index];
			adjusted_trim.m_tolerance[0] = std::max(
			    adjusted_trim.m_tolerance[0], collapse_tolerance);
			adjusted_trim.m_tolerance[1] = std::max(
			    adjusted_trim.m_tolerance[1], collapse_tolerance);
			if (adjusted_trim.m_ei >= 0 &&
				adjusted_trim.m_ei < brep->m_E.Count())
			    brep->m_E[adjusted_trim.m_ei].m_tolerance = std::max(
				brep->m_E[adjusted_trim.m_ei].m_tolerance,
				collapse_tolerance);
		    }
		    std::string diagnostic =
			"source boundary asserted as one topology vertex exceeded the "
			"declared tolerance; used a densely measured OpenNURBS tolerance";
		    if (wrapper->Verbose()) {
			std::ostringstream detail;
			detail << diagnostic << "; measured boundary distance "
			    << measured_collapse_distance << ", effective tolerance "
			    << collapse_tolerance << ", bounded repair limit "
			    << collapse_adjustment_limit;
			diagnostic = detail.str();
		    }
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning, entity_id,
			entity_type, "edge_loop", diagnostic);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"adjusted one source-collapsed boundary tolerance after dense validation");
	}
	/* A supplied singular trim may already connect this pole to a periodic
	 * image which is lift-equivalent but does not meet the following ordinary
	 * trim in parameter space.  Appending another connector in that case makes
	 * the loop continuous, but overlaps part of the existing collapsed-side
	 * constraint.  OpenNURBS accepts the BREP because both curves lift to the
	 * same vertex; shaded CDT correctly rejects the duplicate constraints.
	 * Retarget the existing singular connector instead, retaining its endpoint
	 * at the other neighbour and replacing only the arbitrary pole parameter. */
	const bool first_is_singular = first->m_type == ON_BrepTrim::singular;
	const bool second_is_singular = second->m_type == ON_BrepTrim::singular;
	if (first_is_singular != second_is_singular) {
	    const int existing_index = first_is_singular ?
		first->m_trim_index : second->m_trim_index;
	    ON_3dPoint replacement_start = first_is_singular ?
		first->PointAtStart() : first_end;
	    ON_3dPoint replacement_end = first_is_singular ?
		second_start : second->PointAtEnd();
	    std::unique_ptr<ON_LineCurve> replacement(
		new ON_LineCurve(replacement_start, replacement_end));
	    bool replacement_exact = existing_index >= 0 &&
		existing_index < brep->m_T.Count() &&
		replacement->ChangeDimension(2) && replacement->IsValid();
	    ON_Surface::ISO replacement_iso = ON_Surface::not_iso;
	    if (replacement_exact) {
		replacement_iso = surface->IsIsoparametric(*replacement);
		replacement_exact = replacement_iso == singular_iso;
	    }
	    for (int sample = 0; replacement_exact &&
		    sample <= kDenseValidationSegments; ++sample) {
		const ON_3dPoint uv = replacement->PointAt(
		    replacement->Domain().ParameterAt(
			static_cast<double>(sample) /
			kDenseValidationSegments));
		const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		replacement_exact = lift.IsValid() &&
		    lift.DistanceTo(vertex) <= collapse_tolerance;
	    }
	    if (replacement_exact) {
		const int c2_index = brep->AddTrimCurve(replacement.release());
		ON_BrepTrim &existing = brep->m_T[existing_index];
		if (c2_index >= 0 && brep->SetTrimCurve(existing, c2_index)) {
		    existing.m_iso = replacement_iso;
		    existing.m_tolerance[0] = collapse_tolerance;
		    existing.m_tolerance[1] = collapse_tolerance;
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"retargeted an existing singular trim to close the exact pole boundary");
		    changed = true;
		    break;
		}
	    }
	}
	if (changed)
	    break;
	std::vector<int> original_trims;
		original_trims.reserve(original_count);
		for (int offset = 0; offset < original_count; ++offset)
		    original_trims.push_back(loop.m_ti[offset]);
		const int c2_index = brep->AddTrimCurve(singular_curve.release());
		if (c2_index < 0)
		    continue;
		const int singular_index = brep->NewSingularTrim(
		    brep->m_V[vertex_index], loop, singular_iso, c2_index).m_trim_index;
		brep->m_T[singular_index].m_tolerance[0] = collapse_tolerance;
		brep->m_T[singular_index].m_tolerance[1] = collapse_tolerance;
		loop.m_ti.SetCount(0);
		for (int offset = 0; offset < original_count; ++offset) {
		    loop.m_ti.Append(original_trims[offset]);
		    if (offset == lti)
			loop.m_ti.Append(singular_index);
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    measured_tolerance_adjustment ?
		    "inserted a measured-tolerance singular trim for a source-asserted collapsed boundary" :
		    "inserted an exact singular trim at a surface pole");
		changed = true;
		break;
	    }
	    if (changed)
		changed_loop = li;
	}
	if (!changed)
	    break;
	/* A singular connector changes only its own loop in the common case.
	 * Restarting at loop zero after every insertion made a face near the end
	 * of a large AP242 shell rescan all preceding faces once per missing pole.
	 * A less common repair can install a private restricted surface for the
	 * complete face, so revisit every loop on that face, but retain the scan
	 * frontier across unrelated faces which were already proven complete. */
	next_loop = changed_loop;
	if (changed_loop >= 0 && changed_loop < brep->m_L.Count()) {
	    const int changed_face = brep->m_L[changed_loop].m_fi;
	    if (changed_face >= 0 && changed_face < brep->m_F.Count()) {
		const ON_BrepFace &face = brep->m_F[changed_face];
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int sibling_loop = face.m_li[fli];
		    if (sibling_loop >= 0 && sibling_loop < next_loop)
			next_loop = sibling_loop;
		}
	    }
	}
	if (next_loop < 0 || next_loop >= brep->m_L.Count())
	    break;
    }
}


/* Periodic branch normalization can make a singular connector which was
 * needed earlier in the transaction redundant: its two ordinary neighbours
 * now meet at the same parameter image, while the old connector winds away
 * from and back to that image along a collapsed surface side.  OpenNURBS
 * correctly rejects such a singular trim because its pcurve is closed.
 *
 * Remove only a zero-topology connector whose exposed neighbours already
 * meet numerically and whose literal surface images still agree with the
 * shared topology vertex.  This does not discard a STEP edge (singular trims
 * have none), and it cannot erase a required pole-side span because any
 * distinct neighbouring parameter images fail the exposed-join test. */
size_t
remove_redundant_closed_singular_trims(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t removed = 0;
    const int repair_budget = brep->m_T.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    const int trim_count = loop.TrimCount();
	    if (!surface || trim_count < 2)
		continue;
	    for (int lti = 0; lti < trim_count; ++lti) {
		ON_BrepTrim *trim = loop.Trim(lti);
		const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
		if (!trim || !curve || trim->m_type != ON_BrepTrim::singular ||
			trim->m_ei >= 0 || trim->m_vi[0] < 0 ||
			trim->m_vi[0] != trim->m_vi[1] ||
			trim->m_vi[0] >= brep->m_V.Count() ||
			!curve->IsClosed())
		    continue;
		ON_BrepTrim *previous = loop.Trim(
		    (lti + trim_count - 1) % trim_count);
		ON_BrepTrim *next = loop.Trim((lti + 1) % trim_count);
		if (!previous || !next || previous == trim || next == trim ||
			previous->m_vi[1] != trim->m_vi[0] ||
			next->m_vi[0] != trim->m_vi[1] ||
			previous->m_vi[1] != next->m_vi[0])
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		if (!previous_uv.IsValid() || !next_uv.IsValid() ||
			previous_uv.DistanceTo(next_uv) > ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint previous_lift =
		    surface->PointAt(previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift =
		    surface->PointAt(next_uv.x, next_uv.y);
		const ON_3dPoint &vertex =
		    brep->m_V[trim->m_vi[0]].point;
		double tolerance = std::max(LocalUnits::tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(previous->m_tolerance[0],
			previous->m_tolerance[1]));
		tolerance = std::max(tolerance,
		    std::max(next->m_tolerance[0], next->m_tolerance[1]));
		if (previous->Edge())
		    tolerance = std::max(tolerance,
			previous->Edge()->m_tolerance);
		if (next->Edge())
		    tolerance = std::max(tolerance,
			next->Edge()->m_tolerance);
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > tolerance ||
			next_lift.DistanceTo(vertex) > tolerance ||
			previous_lift.DistanceTo(next_lift) > tolerance)
		    continue;

		const int trim_index = trim->m_trim_index;
		brep->DeleteTrim(brep->m_T[trim_index], true);
		if (!brep->Compact())
		    return removed;
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "removed a redundant closed singular connector after exact "
		    "periodic branch closure");
		++removed;
		changed = true;
		break;
	    }
	}
	if (!changed)
	    break;
    }
    return removed;
}


bool
validate_periodic_trim_translation(const ON_Surface *surface,
	const ON_BrepTrim &original, const ON_Curve &candidate,
	std::string *failure, double bounded_lift_tolerance,
	double *measured_bounded_mismatch)
{
    if (failure)
	failure->clear();
    if (measured_bounded_mismatch)
	*measured_bounded_mismatch = 0.0;
    if (!surface)
	return false;
    const ON_Interval original_domain = original.Domain();
    const ON_Interval candidate_domain = candidate.Domain();
    if (!original_domain.IsIncreasing() || !candidate_domain.IsIncreasing()) {
	if (failure)
	    *failure = "original or translated parameter domain was invalid";
	return false;
    }
    const int validation_spans = std::max(original.SpanCount(),
	candidate.SpanCount());
    const int samples = std::min(4096, std::max(64, validation_spans * 8));
    /* A whole-period translation is an exact parameter-branch operation, not
     * a source-geometry repair.  Validate the literal OpenNURBS evaluation at
     * numerical precision; a model-tolerance allowance here can hide an
     * out-of-domain curve whose interior evaluates on the wrong branch. */
    const double exact_lift_tolerance = bounded_lift_tolerance > 0.0 ?
	bounded_lift_tolerance : std::max(
	ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	LocalUnits::tolerance * 1.0e-8);
    const ON_BrepEdge *edge = bounded_lift_tolerance > 0.0 ?
	original.Edge() : NULL;
    ON_NurbsCurve edge_nurbs;
    if (bounded_lift_tolerance > 0.0 &&
	    (!edge || !edge->GetNurbForm(edge_nurbs))) {
	if (failure)
	    *failure = "bounded periodic translation lacked an exact edge";
	return false;
    }
    const ON_Interval edge_domain = edge ? edge_nurbs.Domain() :
	ON_Interval::EmptyInterval;
    double maximum_bounded_mismatch = 0.0;
    for (int sample = 0; sample <= samples; ++sample) {
	if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
	    if (failure)
		*failure = "periodic translation validation was cancelled";
	    return false;
	}
	const double fraction = static_cast<double>(sample) / samples;
	const ON_3dPoint original_uv = original.PointAt(
	    original_domain.ParameterAt(fraction));
	const ON_3dPoint candidate_uv = candidate.PointAt(
	    candidate_domain.ParameterAt(fraction));
	/* This predicate approves a curve for final storage in the BREP.
	 * OpenNURBS subsequently evaluates that stored pcurve with the surface's
	 * raw PointAt implementation, not our closed-surface modulo adapter.  A
	 * mathematically equivalent full-period translation can therefore still
	 * be invalid when a closed, non-periodic NURBS surface extrapolates
	 * outside its native domain.  Validate the representation OpenNURBS will
	 * actually consume; modulo evaluation is appropriate only for temporary
	 * branch searches whose result is returned to the native domain before
	 * installation. */
	/* An intermediate pcurve may intentionally carry an unwrapped image
	 * outside a closed NURBS surface's native knot domain.  Raw PointAt()
	 * extrapolates there and is not the mathematical periodic image.  In the
	 * bounded safe-repair path, compare that source image modulo the closed
	 * domain and require the final, literally evaluated candidate to agree
	 * both with it and with the immutable 3-D edge below.  The ordinary exact
	 * path retains the raw-evaluation equivalence test. */
	const ON_3dPoint original_lift =
	    bounded_lift_tolerance > 0.0 &&
	    has_unwrapped_closed_parameter(surface, original_uv) ?
	    closed_surface_point_at(surface, original_uv) :
	    surface->PointAt(original_uv.x, original_uv.y);
	const ON_3dPoint candidate_lift =
	    surface->PointAt(candidate_uv.x, candidate_uv.y);
	const double lift_distance =
	    original_lift.IsValid() && candidate_lift.IsValid() ?
	    original_lift.DistanceTo(candidate_lift) : DBL_MAX;
	maximum_bounded_mismatch = std::max(maximum_bounded_mismatch,
	    lift_distance);
	if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
		lift_distance > exact_lift_tolerance) {
	    if (measured_bounded_mismatch)
		*measured_bounded_mismatch = maximum_bounded_mismatch;
	    if (failure) {
		std::ostringstream reason;
		reason << "surface lift changed at sample " << sample << " by "
		    << std::setprecision(17) << lift_distance;
		*failure = reason.str();
	    }
	    return false;
	}
	if (edge) {
	    const ON_3dPoint edge_point = edge_nurbs.PointAt(
		edge_domain.ParameterAt(original.m_bRev3d ?
		    1.0 - fraction : fraction));
	    double edge_distance = edge_point.IsValid() ?
		candidate_lift.DistanceTo(edge_point) : DBL_MAX;
	    if (edge_distance > bounded_lift_tolerance && sample > 0 &&
		    sample < samples) {
		double edge_parameter = 0.0;
		if (ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
			candidate_lift))
		    edge_distance = std::min(edge_distance,
			candidate_lift.DistanceTo(
			    edge_nurbs.PointAt(edge_parameter)));
	    }
	    maximum_bounded_mismatch = std::max(maximum_bounded_mismatch,
		edge_distance);
	    if (edge_distance > bounded_lift_tolerance) {
		if (measured_bounded_mismatch)
		    *measured_bounded_mismatch = maximum_bounded_mismatch;
		if (failure) {
		    std::ostringstream reason;
		    reason << "translated surface lift left its exact edge at "
			"sample " << sample << " by " << std::setprecision(17)
			<< edge_distance;
		    *failure = reason.str();
		}
		return false;
	    }
	}
    }
    if (measured_bounded_mismatch)
	*measured_bounded_mismatch = maximum_bounded_mismatch;
    return true;
}


size_t
repair_ambiguous_singular_periodic_loop_branches(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_loops = 0;
    bool modified_curve_array = false;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	const ON_BrepLoop::TYPE expected = loop.m_type;
	if (!surface || loop.TrimCount() < 4 ||
		(expected != ON_BrepLoop::outer &&
		 expected != ON_BrepLoop::inner) ||
		brep->ComputeLoopType(loop) == expected)
	    continue;

	std::vector<int> singular_positions;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::singular)
		singular_positions.push_back(lti);
	}
	if (singular_positions.size() < 2)
	    continue;

	bool repaired = false;
	for (size_t singular = 0; singular < singular_positions.size() && !repaired;
		singular++) {
	    const int first_position = singular_positions[singular];
	    const int second_position = singular_positions[
		(singular + 1) % singular_positions.size()];
	    ON_BrepTrim *first_singular = loop.Trim(first_position);
	    ON_BrepTrim *second_singular = loop.Trim(second_position);
	    if (!first_singular || !second_singular ||
		first_singular->m_vi[0] != first_singular->m_vi[1] ||
		second_singular->m_vi[0] != second_singular->m_vi[1])
		continue;

	    const auto varying_direction = [](ON_Surface::ISO iso) {
		switch (iso) {
		    case ON_Surface::S_iso:
		    case ON_Surface::N_iso: return 0;
		    case ON_Surface::E_iso:
		    case ON_Surface::W_iso: return 1;
		    default: return -1;
		}
	    };
	    const int direction = varying_direction(first_singular->m_iso);
	    if (direction < 0 ||
		varying_direction(second_singular->m_iso) != direction ||
		!surface->IsClosed(direction))
		continue;
	    const double period = surface->Domain(direction).Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		continue;

	    std::vector<int> arc_positions;
	    for (int position = (first_position + 1) % loop.TrimCount();
		    position != second_position;
		    position = (position + 1) % loop.TrimCount()) {
		ON_BrepTrim *trim = loop.Trim(position);
		if (!trim || trim->m_type == ON_BrepTrim::singular) {
		    arc_positions.clear();
		    break;
		}
		arc_positions.push_back(position);
	    }
	    if (arc_positions.empty())
		continue;

	    std::vector<int> affected_positions;
	    affected_positions.reserve(arc_positions.size() + 2);
	    affected_positions.push_back(first_position);
	    affected_positions.insert(affected_positions.end(),
		arc_positions.begin(), arc_positions.end());
	    affected_positions.push_back(second_position);

	    for (int shift_sign = -1; shift_sign <= 1 && !repaired;
		    shift_sign += 2) {
		std::vector<std::unique_ptr<ON_Curve> > originals;
		originals.reserve(affected_positions.size());
		bool saved = true;
		for (size_t affected = 0; affected < affected_positions.size();
			affected++) {
		    ON_BrepTrim *trim = loop.Trim(affected_positions[affected]);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    if (!curve) {
			saved = false;
			break;
		    }
		    originals.emplace_back(curve);
		}
		if (!saved)
		    continue;

		std::vector<std::unique_ptr<ON_Curve> > candidates;
		candidates.reserve(arc_positions.size());
		bool exact = true;
		ON_Xform translation(ON_Xform::IdentityTransformation);
		translation.m_xform[direction][3] = shift_sign * period;
		for (size_t arc = 0; arc < arc_positions.size(); ++arc) {
		    ON_BrepTrim *trim = loop.Trim(arc_positions[arc]);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    std::string failure;
		    if (!curve || !curve->Transform(translation) ||
			    !curve->ChangeDimension(2) || !curve->IsValid() ||
			    !validate_periodic_trim_translation(surface, *trim,
				*curve, &failure)) {
			delete curve;
			exact = false;
			break;
		    }
		    candidates.emplace_back(curve);
		}
		if (!exact || candidates.size() != arc_positions.size())
		    continue;

		const ON_3dPoint first_start = first_singular->PointAtStart();
		const ON_3dPoint first_end = candidates.front()->PointAtStart();
		const ON_3dPoint second_start = candidates.back()->PointAtEnd();
		const ON_3dPoint second_end = second_singular->PointAtEnd();
		std::unique_ptr<ON_LineCurve> first_connector(
		    new ON_LineCurve(first_start, first_end));
		std::unique_ptr<ON_LineCurve> second_connector(
		    new ON_LineCurve(second_start, second_end));
		if (!first_connector->ChangeDimension(2) ||
			!second_connector->ChangeDimension(2) ||
			!first_connector->IsValid() || !second_connector->IsValid())
		    continue;

		const auto singular_connector_is_exact = [brep, surface](
			const ON_BrepTrim &trim, const ON_Curve &curve) {
		    if (trim.m_vi[0] < 0 || trim.m_vi[0] >= brep->m_V.Count())
			return false;
		    const ON_3dPoint &vertex = brep->m_V[trim.m_vi[0]].point;
		    double tolerance = std::max(LocalUnits::tolerance,
			brep->m_V[trim.m_vi[0]].m_tolerance);
		    tolerance = std::max(tolerance,
			std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
		    for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
			    ++sample) {
			const ON_3dPoint uv = curve.PointAt(curve.Domain().ParameterAt(
			    static_cast<double>(sample) /
			    kPcurveLocusScreeningSegments));
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			if (!lift.IsValid() || lift.DistanceTo(vertex) > tolerance)
			    return false;
		    }
		    return true;
		};
		if (!singular_connector_is_exact(*first_singular,
			*first_connector) ||
			!singular_connector_is_exact(*second_singular,
			    *second_connector))
		    continue;

		std::vector<ON_Curve *> installed_candidates;
		installed_candidates.reserve(affected_positions.size());
		installed_candidates.push_back(first_connector.release());
		for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
		    installed_candidates.push_back(candidates[candidate].release());
		installed_candidates.push_back(second_connector.release());
		bool installed = true;
		size_t affected = 0;
		for (; affected < affected_positions.size();
			affected++) {
		    ON_BrepTrim *trim = loop.Trim(affected_positions[affected]);
		    ON_Curve *curve = installed_candidates[affected];
		    const int c2_index = brep->AddTrimCurve(curve);
		    if (c2_index < 0 || !trim || !brep->SetTrimCurve(*trim,
			    c2_index)) {
			if (c2_index < 0)
			    delete curve;
			installed = false;
			break;
		    }
		    modified_curve_array = true;
		    brep->SetTrimIsoFlags(*trim);
		}
		/* AddTrimCurve takes ownership only of curves it accepts.  A partial
		 * installation can therefore leave the unvisited tail in this local
		 * raw-pointer array; release that tail explicitly before restoring the
		 * saved chain. */
		if (!installed) {
		    for (size_t remaining = affected + 1;
			    remaining < installed_candidates.size(); ++remaining)
			delete installed_candidates[remaining];
		}

		const ON_BrepLoop::TYPE candidate_type = installed ?
		    brep->ComputeLoopType(loop) : ON_BrepLoop::unknown;
		if (installed && candidate_type == expected) {
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"selected the exact singular-pole branch matching the STEP face bound");
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << "/STEP" << loop.m_loop_user.i
			    << " selected singular-pole branch shift "
			    << shift_sign << "*" << period << " in direction "
			    << direction << " to retain bound type "
			    << static_cast<int>(expected) << std::endl;
		    repaired = true;
		    ++repaired_loops;
		    continue;
		}

		/* The trial changes parameter images only.  Restore the complete
		 * affected chain if it did not recover the authoritative STEP bound
		 * classification; unused trial curves are compacted with the BREP. */
		for (size_t restore_index = 0;
			restore_index < affected_positions.size(); ++restore_index) {
		    ON_BrepTrim *trim = loop.Trim(
			affected_positions[restore_index]);
		    ON_Curve *curve = originals[restore_index].release();
		    const int c2_index = brep->AddTrimCurve(curve);
		    if (c2_index < 0) {
			delete curve;
			continue;
		    }
		    modified_curve_array = true;
		    if (trim && brep->SetTrimCurve(*trim, c2_index))
			brep->SetTrimIsoFlags(*trim);
		}
	    }
	}
    }
    /* Branch trials replace pcurves transactionally but AddTrimCurve retains
     * the superseded candidates until compaction.  Remove those unreferenced
     * curves once, after all loop indices and references above are finished. */
    if (modified_curve_array && !brep->Compact() && wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id
	    << ": could not compact superseded singular-branch pcurves"
	    << std::endl;
    return repaired_loops;
}


/* Return a private pcurve translated by exact integral surface periods so its
 * selected endpoint lies on the parameter branch of join.  A small residual
 * endpoint discrepancy is deliberately left for the ordinary, densely
 * validated endpoint repair; accepting it here would turn a parameter branch
 * recognition bound into a geometric tolerance. */
ON_Curve *
translated_periodic_trim_for_join(const ON_Surface *surface,
    const ON_BrepTrim &trim, const ON_3dPoint &join, bool move_start,
    std::string *failure, double bounded_lift_tolerance,
    double *measured_bounded_mismatch)
{
    if (failure)
	failure->clear();
    if (!surface || !join.IsValid())
	return NULL;

    const ON_3dPoint endpoint = move_start ? trim.PointAtStart() :
	trim.PointAtEnd();
    ON_3dVector translation = join - endpoint;
    bool shifted = false;
    bool bounded_parameter_residual = false;
    for (int axis = 0; axis < 2; ++axis) {
	const double period = surface->Domain(axis).Length();
	const double parameter_window = std::max(ON_ZERO_TOLERANCE,
	    kPeriodicParameterSnapFraction * std::max(1.0, fabs(period)));
	if (fabs(translation[axis]) <= parameter_window) {
	    translation[axis] = 0.0;
	    continue;
	}
	if (!surface->IsClosed(axis) || !(period > ON_ZERO_TOLERANCE)) {
	    /* A whole-period shift in the closed coordinate can be accompanied
	     * by a small residual in the transverse, open coordinate.  Safe mode
	     * reaches this helper only after both endpoints and their shared STEP
	     * vertex have passed the bounded model-space join proof.  Preserve
	     * that transverse coordinate here and let the ordinary endpoint
	     * transaction reconcile it after the exact integral shift.  The final
	     * residual lift check below remains authoritative; exact mode supplies
	     * no bounded tolerance and retains the strict rejection. */
	    if (bounded_lift_tolerance > 0.0) {
		translation[axis] = 0.0;
		bounded_parameter_residual = true;
		continue;
	    }
	    if (failure)
		*failure = "the endpoint difference was not on a closed surface direction";
	    return NULL;
	}
	const double periods = round(translation[axis] / period);
	if (fabs(periods) < 0.5) {
	    /* A whole-period shift in the other closed direction can leave a
	     * small residual in this coordinate.  Safe mode has already proven
	     * the shared topology vertex in model space, so preserve this axis
	     * and let the bounded endpoint transaction reconcile it after the
	     * integral shift. */
	    if (bounded_lift_tolerance > 0.0) {
		translation[axis] = 0.0;
		bounded_parameter_residual = true;
		continue;
	    }
	    if (failure)
		*failure = "the endpoint difference was not within the periodic branch window";
	    return NULL;
	}
	if (fabs(translation[axis] - periods * period) > parameter_window) {
	    /* Safe mode may already have established a local edge/vertex
	     * tolerance which proves both pcurve endpoints belong to the same
	     * STEP topology vertex.  In that case recognize only the integral
	     * part here and leave the residual for the separately validated
	     * endpoint repair.  The complete translated pcurve is checked against
	     * its immutable 3-D edge below; exact mode supplies no bounded
	     * tolerance and retains the strict parameter window. */
	    if (!(bounded_lift_tolerance > 0.0)) {
		if (failure)
		    *failure = "the endpoint difference was not within the periodic branch window";
		return NULL;
	    }
	    bounded_parameter_residual = true;
	}
	/* Never incorporate the source discrepancy into the transform. */
	translation[axis] = periods * period;
	shifted = true;
    }
    if (!shifted)
	return NULL;

    ON_Curve *translated = trim.DuplicateCurve();
    ON_Xform transform(ON_Xform::IdentityTransformation);
    transform.m_xform[0][3] = translation.x;
    transform.m_xform[1][3] = translation.y;
    if (!translated || !translated->Transform(transform) ||
	    !translated->ChangeDimension(2) || !translated->IsValid() ||
	    !validate_periodic_trim_translation(surface, trim, *translated,
		failure, bounded_lift_tolerance,
		measured_bounded_mismatch)) {
	delete translated;
	return NULL;
    }
    if (bounded_parameter_residual) {
	const ON_3dPoint translated_endpoint = endpoint + translation;
	const ON_3dPoint translated_lift = surface->PointAt(
	    translated_endpoint.x, translated_endpoint.y);
	const ON_3dPoint join_lift = closed_surface_point_at(surface, join);
	if (!translated_lift.IsValid() || !join_lift.IsValid() ||
		translated_lift.DistanceTo(join_lift) >
		    bounded_lift_tolerance) {
	    if (failure)
		*failure =
		    "the residual periodic-branch endpoint mismatch exceeded its "
		    "validated model-space tolerance";
	    delete translated;
	    return NULL;
	}
    }
    return translated;
}


double
verified_source_pcurve_tolerance(ON_BrepTrim &trim,
	ON_BrepEdge &edge, const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance,
	const ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!trim.TrimCurveOf() || !surface || !wrapper ||
	    wrapper->ImportOptions().exact ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    !(tolerance > 0.0))
	return tolerance;

    const double adjusted = measured_source_pcurve_tolerance(trim,
	surface, edge_nurbs, tolerance, brep);
    if (!(adjusted > tolerance))
	return tolerance;

    edge.m_tolerance = std::max(edge.m_tolerance, adjusted);
    trim.m_tolerance[0] = std::max(trim.m_tolerance[0], adjusted);
    trim.m_tolerance[1] = std::max(trim.m_tolerance[1], adjusted);
    if (wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id << ": trim "
	    << trim.m_trim_index << "/STEP edge " << edge.m_edge_user.i
	    << " declared/existing tolerance=" << tolerance
	    << " adjusted tolerance=" << adjusted << std::endl;
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	entity_id, entity_type, "trim_pcurve",
	"existing pcurve and 3-D edge exceeded the declared tolerance after "
	"exact regeneration failed; "
	"used a densely measured OpenNURBS tolerance");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"adjusted one trim tolerance to densely measured existing pcurve geometry");
    return adjusted;
}


double
verified_regeneration_tolerance(ON_BrepTrim &trim,
	ON_BrepEdge &edge, const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance,
	const ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!surface || !wrapper || wrapper->ImportOptions().exact ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    !(tolerance > 0.0))
	return tolerance;

    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge_nurbs.Domain();
    const ON_BoundingBox bounds = edge_nurbs.BoundingBox();
    const double scale = bounds.IsValid() ? bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep && brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const double limit = std::max(tolerance,
	std::max(scale * kRegenerationMaximumRelativeMismatch,
	    item_scale * kRegenerationMaximumRelativeItemMismatch));
    const double solver_tolerance = std::max(
	ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance * 0.1);
    double measured = 0.0;
    for (int sample = 0;
	    sample <= kRegenerationMeasurementSegments; ++sample) {
	if (brlcad::PullbackWorkCancelled())
	    return tolerance;
	const double fraction = static_cast<double>(sample) /
	    kRegenerationMeasurementSegments;
	const ON_3dPoint uv = trim.PointAt(trim_domain.ParameterAt(fraction));
	const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	const ON_3dPoint edge_point = edge_nurbs.PointAt(
	    edge_domain.ParameterAt(trim.m_bRev3d ? 1.0 - fraction : fraction));
	if (!lift.IsValid() || !edge_point.IsValid())
	    return tolerance;
	const double corresponding_distance = lift.DistanceTo(edge_point);
	/* The supplied pcurve is a useful projection seed and an upper bound.  If
	 * that upper bound is already inside the tolerance established so far, no
	 * closest-surface solve can increase the measured source separation. */
	if (corresponding_distance <= std::max(tolerance, measured))
	    continue;
	ON_3dPoint projected_uv = uv;
	double surface_distance = DBL_MAX;
	if (!refine_surface_pullback_seeded(surface, edge_point,
		solver_tolerance, projected_uv, &surface_distance, false, limit))
	    return tolerance;
	measured = std::max(measured, surface_distance);
    }
    if (!(measured > tolerance))
	return tolerance;

    const double adjusted = measured * kRegenerationToleranceSafety;
    if (!(adjusted <= limit)) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": trim "
		<< trim.m_trim_index << "/STEP edge " << edge.m_edge_user.i
		<< " regeneration measured source "
		<< "mismatch " << measured << " but bounded adjustment "
		<< adjusted << " exceeds limit " << limit << std::endl;
	return tolerance;
    }

    edge.m_tolerance = std::max(edge.m_tolerance, adjusted);
    trim.m_tolerance[0] = std::max(trim.m_tolerance[0], adjusted);
    trim.m_tolerance[1] = std::max(trim.m_tolerance[1], adjusted);
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	entity_id, entity_type, "trim_pcurve",
	"source edge/surface separation exceeded the declared tolerance; "
	"used a densely measured tolerance for exact pcurve regeneration");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"adjusted one trim tolerance to measured source geometry");
    return adjusted;
}

} /* namespace step_brep_detail */
