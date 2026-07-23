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
/** @file step/STEPWrapperBrep.inc
 *
 * Exact BREP repair, validation, and deterministic output helpers shared by
 * the schema-specific STEP importers.  Included only by STEPWrapper.cpp.
 */

static ON_3dPoint closed_surface_point_at(const ON_Surface *surface,
    const ON_3dPoint &uv);

static bool
remove_adjacent_zero_area_slit(ON_Brep *brep, int loop_index)
{
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
		first->m_bRev3d == second->m_bRev3d ||
		first->m_ei >= brep->m_E.Count() ||
		brep->m_E[first->m_ei].m_edge_user.i <= 0 ||
		brep->m_E[first->m_ei].m_ti.Count() != 2 ||
		first->m_vi[0] != second->m_vi[1] ||
		first->m_vi[1] != second->m_vi[0])
	    return false;
	const ON_Interval first_domain = first->Domain();
	const ON_Interval second_domain = second->Domain();
	const ON_Surface *surface = face->SurfaceOf();
	if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing() ||
		!surface)
	    return false;
	const ON_BrepEdge &edge = brep->m_E[first->m_ei];
	const ON_Interval edge_domain = edge.Domain();
	if (!edge_domain.IsIncreasing())
	    return false;
	double tolerance = std::max(LocalUnits::tolerance, edge.m_tolerance);
	tolerance = std::max(tolerance, std::max(first->m_tolerance[0],
	    first->m_tolerance[1]));
	tolerance = std::max(tolerance, std::max(second->m_tolerance[0],
	    second->m_tolerance[1]));
	for (int sample = 0; sample <= kPcurveLocusScreeningSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kPcurveLocusScreeningSegments;
	    const ON_3dPoint first_uv = first->PointAt(
		first_domain.ParameterAt(fraction));
	    const ON_3dPoint second_uv = second->PointAt(
		second_domain.ParameterAt(1.0 - fraction));
	    const ON_3dPoint first_lift =
		surface->PointAt(first_uv.x, first_uv.y);
	    const ON_3dPoint second_lift =
		surface->PointAt(second_uv.x, second_uv.y);
	    const ON_3dPoint first_edge_point = edge.PointAt(
		edge_domain.ParameterAt(first->m_bRev3d ?
		    1.0 - fraction : fraction));
	    const ON_3dPoint second_edge_point = edge.PointAt(
		edge_domain.ParameterAt(second->m_bRev3d ?
		    fraction : 1.0 - fraction));
	    if (!first_uv.IsValid() || !second_uv.IsValid() ||
		    !first_lift.IsValid() || !second_lift.IsValid() ||
		    !first_edge_point.IsValid() || !second_edge_point.IsValid() ||
		    first_lift.DistanceTo(first_edge_point) > tolerance ||
		    second_lift.DistanceTo(second_edge_point) > tolerance ||
		    first_lift.DistanceTo(second_lift) > tolerance)
		return false;
	}
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


static SingularLoopClosure
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


static bool
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
	const int other = 1 - direction;
	const double period = surface->Domain(direction).Length();
	const double period_tolerance = parameter_tolerance *
	    std::max(1.0, fabs(period));
	if (fabs(fabs(last_uv[direction] - first_uv[direction]) - period) <=
		period_tolerance &&
		fabs(last_uv[other] - first_uv[other]) <= parameter_tolerance) {
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


static bool
opposite_trim_curves_coincide(const ON_BrepTrim *first,
    const ON_BrepTrim *second, double parameter_tolerance)
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
	if (!first_uv.IsValid() || !second_uv.IsValid() ||
		first_uv.DistanceTo(second_uv) > parameter_tolerance)
	    return false;
    }
    return true;
}


static bool
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
		first_offset = first;
		second_offset = second;
		insert_inside_singular = candidate_inside_singular.valid;
		insert_outside_singular = candidate_outside_singular.valid;
		inside_periodic = candidate_inside_periodic;
		outside_periodic = candidate_outside_periodic;
		remove_periodic_duplicate_only = candidate_inside_periodic &&
		    candidate_outside_periodic;
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
	if (failure_reason && rejected_candidate_count)
	    *failure_reason += "; rejected candidates: " +
		rejected_candidates.str();
	return false;
    }

    if (remove_periodic_duplicate_only) {
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
	return brep->Compact();
    }

    const ON_BrepLoop::TYPE original_type = loop->m_type;
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
	const int surface_index = face->m_si;
	const bool reversed = face->m_bRev;
	ON_BrepFace &new_face = brep->NewFace(surface_index);
	new_face.m_bRev = reversed;
	new_face.m_li.Append(new_loop_index);
	brep->m_L[new_loop_index].m_fi = new_face.m_face_index;
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
    return brep->Compact();
}


static size_t
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


static void
refresh_brep_flags_preserving_singular_isos(ON_Brep *brep,
	bool set_loop_type, STEPWrapper *wrapper = NULL, int entity_id = 0,
	const std::string *entity_type = NULL)
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
static bool
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


/* Some STEP writers encode the two sides of a periodic surface seam as two
 * distinct EDGE_CURVEs with identical vertices and identical 3-D geometry.
 * Each edge then has one boundary use, so an otherwise closed OpenNURBS BREP
 * remains open.  Normalize one such pair only after proving that the trims are
 * in the same loop on complementary native sides and that the complete edge
 * loci agree within the asserted model tolerance.  The candidate remains
 * transactional through reciprocal-reference and OpenNURBS validation. */
static size_t
merge_one_exact_periodic_boundary_edge_pair(ON_Brep *brep,
	std::string *details)
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
		if (!brep_topology_references_are_safe(candidate.get(),
			&unsafe_topology) || !candidate->IsValid(&validation_log)) {
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


static bool
regenerate_collapsed_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	double *proven_open_parameter = NULL)
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
    if (!supplied_start.IsValid() || !supplied_end.IsValid() ||
	    fabs(supplied_start[open_direction] -
		supplied_end[open_direction]) > parameter_tolerance)
	return false;
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
	bool pulled = edge_point.IsValid() &&
	    pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0,
		std::max(ON_ZERO_TOLERANCE,
		    projection_tolerance * 0.1), projection_tolerance);
	/* The closest-point routine uses within_distance_tol as a search-window
	 * and completion threshold, not merely as an acceptance bound.  A broad
	 * safe-repair limit can therefore stop at the wrong parameter branch on a
	 * small-radius periodic surface.  Search at model tolerance first; only a
	 * genuinely separated source edge gets the broader bounded second pass. */
	if (!pulled && edge_point.IsValid())
	    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0,
		std::max(ON_ZERO_TOLERANCE,
		    projection_tolerance * 0.1), adjustment_limit);
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
static bool
regenerate_native_seam_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, double fixed_parameter,
	bool record_repair = true)
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
	    native_min_lift.DistanceTo(vertex) > topology_tolerance ||
	    native_max_lift.DistanceTo(vertex) > topology_tolerance)
	return false;

    const double period = closed_domain.Length();
    const ON_BoundingBox edge_bounds = edge->BoundingBox();
    const double edge_feature_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    brlcad::PullbackContext pullback_context;
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
	    bool pulled = edge_point.IsValid() &&
		pullback_context.SurfaceClosestPoint(surface, edge_point,
		    pulled_uv, pulled_lift, pulled_distance, 0,
		    closest_point_tolerance, topology_tolerance);
	    if (!pulled && edge_point.IsValid() &&
		    edge_feature_scale > ON_ZERO_TOLERANCE) {
		/* A fixed micro-scale solver threshold can fail to converge on source
		 * geometry whose independently measured mismatch is much larger.  Retry
		 * at a feature-relative threshold, capped at one ten-thousandth of
		 * this edge so adjacent boundary samples cannot collapse together. */
		const double feature_solver_tolerance = std::max(
		    closest_point_tolerance,
		    std::min(topology_tolerance * 0.1,
			edge_feature_scale * kPullbackSolverFeatureFraction));
		if (feature_solver_tolerance > closest_point_tolerance)
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pulled_distance, 0,
			feature_solver_tolerance, topology_tolerance);
	    }
	    if (!pulled) {
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
	    if (!lift.IsValid() ||
		    lift.DistanceTo(edge_point) > topology_tolerance) {
		exact = false;
		failure_reason = "validating the fixed-parameter lift";
		failure_sample = sample;
		failure_distance = lift.IsValid() ?
		    lift.DistanceTo(edge_point) : DBL_MAX;
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
	    exact = lift.IsValid() && edge_point.IsValid() &&
		lift.DistanceTo(edge_point) <= topology_tolerance;
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
		    << failure_distance << " tolerance=" << topology_tolerance
		    << std::endl;
	    continue;
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


static bool
split_periodic_boundary_at_native_seam(ON_Brep *brep,
	int target_loop_index, int target_trim_index, const ON_Surface *surface,
	int closed_direction, double open_parameter, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
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
	    crossing = static_cast<int>(sample);
	    seam_parameter = nearest_seam;
	    sampled_seam_crossing = true;
	    sampled_edge_parameter = projected[sample].edge_parameter;
	    break;
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
    if (crossing < 1) {
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
    for (int use = 0; use < original_edge.m_ti.Count(); ++use)
	original_trim_indices.push_back(original_edge.m_ti[use]);
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
	    const ON_Interval trim_domain = trim.Domain();
	    if (!trim_domain.IsIncreasing())
		return reject();
	    const auto lifted_trim_distance = [&trim, surface,
		    &seam_point](double parameter) {
		const ON_3dPoint uv = trim.PointAt(parameter);
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
	    if (!trim.Split(trim_parameter, left_raw, right_raw))
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
	    if (!left_start_lift.IsValid() || !left_end_lift.IsValid() ||
		    !right_start_lift.IsValid() || !right_end_lift.IsValid() ||
		    left_start_lift.DistanceTo(original_start_vertex) >
			topology_tolerance ||
		    right_end_lift.DistanceTo(original_end_vertex) >
			topology_tolerance ||
		    left_end_lift.DistanceTo(seam_point) > topology_tolerance ||
		    right_start_lift.DistanceTo(seam_point) > topology_tolerance)
		return reject();

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

	    const ON_Curve *left_edge = trim.m_bRev3d ? second_curve.get() :
		first_curve.get();
	    const ON_Curve *right_edge = trim.m_bRev3d ? first_curve.get() :
		second_curve.get();
	    const auto validate_piece = [surface, topology_tolerance, wrapper,
		    entity_id, &entity_type, target_loop_index, target_trim_index](
		    const ON_Curve &pcurve, const ON_Curve &edge_curve,
		    bool reversed, const char *piece_name) {
		ON_NurbsCurve edge_nurbs;
		if (!edge_curve.GetNurbForm(edge_nurbs))
		    return false;
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
				<< std::endl;
			return false;
		    }
		}
		return true;
	    };
	    rejection_stage = "densely validating the split target pcurves";
	    if (!left_edge || !right_edge ||
		    !validate_piece(*left, *left_edge, trim.m_bRev3d, "left") ||
		    !validate_piece(*right, *right_edge, trim.m_bRev3d, "right"))
		return reject();
	    if (!trim.m_bRev3d) {
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
		trim.m_bRev3d ? 1.0 - edge_fraction : edge_fraction);
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
		/* A singleton adjacent use can have the same arbitrary seam-based
		 * pcurve start as the target use.  Its seam point is consequently an
		 * endpoint in parameter space even though the authoritative topology
		 * vertex is interior to the closed 3-D edge.  Rebuild the use as two
		 * native-domain isocurves from that projected topology vertex. */
		ON_2dPoint vertex_uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint vertex_lift;
		double vertex_distance = DBL_MAX;
		bool rebuilt_singleton = false;
		if (original_edge_closed && use_loop.TrimCount() == 1 &&
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
			const bool edge_first_moves_increasing = trim.m_bRev3d ?
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
	    if (!trim.m_bRev3d) {
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
	    if (std::min(forward_error, reverse_error) <= topology_tolerance &&
		    (reverse_error < forward_error) != piece->reversed) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact native-seam split L" << replacement->loop
			<< "/T" << replacement->old_trim
			<< " rejected because directed child endpoint proof "
			<< (reverse_error < forward_error)
			<< " contradicted source sense " << piece->reversed
			<< " (errors " << forward_error << '/' << reverse_error
			<< ')' << std::endl;
		return reject();
	    }
	}
    }

    rejection_stage = "installing the split boundary topology";
    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
    const int first_c3 = brep->AddEdgeCurve(first_curve.release());
    const int second_c3 = brep->AddEdgeCurve(second_curve.release());
    if (first_c3 < 0 || second_c3 < 0) {
	*brep = *rollback;
	return false;
    }
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
	    if (c2 < 0 || edge_index < 0 || edge_index >= brep->m_E.Count()) {
		*brep = *rollback;
		return false;
	    }
	    const int new_trim_index = brep->NewTrim(brep->m_E[edge_index],
		piece->reversed, brep->m_L[replacement->loop], c2).m_trim_index;
	    if (new_trim_index < 0) {
		*brep = *rollback;
		return false;
	    }
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
    if (!brep->Compact()) {
	*brep = *rollback;
	return false;
    }
    /* Each replacement trim initially inherits the source trim type while
     * the original closed edge and its complete use graph still exist.  Once
     * the edge is split and the BREP is compacted, that inherited value can
     * be stale (for example, a former seam use can now be a mated subedge on
     * two faces).  Let OpenNURBS derive the authoritative type from the final
     * reciprocal edge/trim references before structural validation. */
    if (!brep->SetTrimTypeFlags(false)) {
	*brep = *rollback;
	return false;
    }
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(brep, &unsafe_topology)) {
	*brep = *rollback;
	return false;
    }
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
	*brep = *rollback;
	return false;
    }
    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	original_edge_closed ?
	"split a closed STEP boundary edge at an exact OpenNURBS periodic seam" :
	"split an open STEP boundary edge at an exact OpenNURBS periodic seam");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"regenerated an implicit full-period boundary from exact split edge uses");
    return true;
}


/* Split an open edge whose continuous pcurve crosses a native periodic
 * boundary in its interior.  A closed face band needs a topology vertex at
 * that cut so its two physical boundaries can share one explicit OpenNURBS
 * seam.  The general splitter above propagates the same exact 3-D split to
 * every use of the STEP edge; this helper only locates a proven interior UV
 * crossing on the requesting face. */
static bool
split_open_periodic_boundary_crossing(ON_Brep *brep, ON_BrepFace &face,
    int loop_index, int closed_direction, double parameter_tolerance,
    STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
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
    if (!have_parameter || fabs(fabs(net_winding) - period) >
	    parameter_tolerance) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": open periodic boundary split L" << loop_index
		<< " rejected because net pcurve winding " << net_winding
		<< " does not equal one period " << period << std::endl;
	return false;
    }

    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	ON_BrepTrim *trim = loop.Trim(lti);
	const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	if (!trim || !edge || !curve || edge->m_vi[0] == edge->m_vi[1])
	    continue;
	const ON_Interval curve_domain = curve->Domain();
	if (!curve_domain.IsIncreasing())
	    continue;
	const double domain_guard = curve_domain.Length() * 1.0e-10;
	const double seam_hit_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale, period * 1.0e-10);
	const int target_trim_index = trim->m_trim_index;
	const int open_direction = 1 - closed_direction;
	const auto try_split = [brep, loop_index, target_trim_index, surface,
		closed_direction, open_direction, wrapper, entity_id,
		&entity_type, curve, &curve_domain, domain_guard](
		double crossing_parameter) {
	    if (crossing_parameter <= curve_domain.Min() + domain_guard ||
		    crossing_parameter >= curve_domain.Max() - domain_guard)
		return false;
	    const ON_3dPoint crossing = curve->PointAt(crossing_parameter);
	    return crossing.IsValid() &&
		split_periodic_boundary_at_native_seam(brep, loop_index,
		    target_trim_index, surface, closed_direction,
		    crossing[open_direction], wrapper, entity_id, entity_type);
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
static size_t
split_supplied_full_period_boundaries(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper)
	return 0;

    const size_t maximum_splits = static_cast<size_t>(
	std::max(1, brep->m_E.Count()));
    size_t split_count = 0;
    bool changed = true;
    while (changed && split_count < maximum_splits) {
	changed = false;
	for (int fi = 0; fi < brep->m_F.Count() && !changed; ++fi) {
	    ON_BrepFace &face = brep->m_F[fi];
	    const ON_Surface *surface = face.SurfaceOf();
	    if (!surface)
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
		    if (!split_open_periodic_boundary_crossing(brep, face, li,
			    direction, parameter_tolerance, wrapper, entity_id,
			    entity_type))
			continue;
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
static bool
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
    const bool allow_measured_tolerance = !wrapper->ImportOptions().exact &&
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    const double adjustment_limit = allow_measured_tolerance ?
	std::max(topology_tolerance,
	    edge_scale * kRegenerationMaximumRelativeMismatch) :
	topology_tolerance;

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
		    bool pulled = edge_point.IsValid() &&
			pullback_context.SurfaceClosestPoint(surface, edge_point,
			    pulled_uv, pulled_lift, pulled_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				topology_tolerance * 0.1), topology_tolerance);
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
		const double closed_parameter_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    kPeriodicParameterSnapFraction * std::max(1.0, period));
		if (exact && fabs(previous_closed_parameter -
			desired_endpoints[piece + 1][closed_direction]) >
		    closed_parameter_tolerance) {
		    failed_piece = piece;
		    failed_sample = kPeriodicBoundaryConstructionSegments;
		    exact = false;
		}
		if (!exact) continue;
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
			<< topology_tolerance << std::endl;
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


static bool
validate_periodic_trim_translation(const ON_Surface *surface,
	const ON_BrepTrim &original, const ON_Curve &candidate,
	std::string *failure, double bounded_lift_tolerance = -1.0,
	double *measured_bounded_mismatch = NULL);

static bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type);

static bool
relocate_closed_surface_loop_seam(ON_Brep *brep, int loop_index,
	int direction, double tolerance, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, std::string *failure_reason,
	double requested_seam =
	    std::numeric_limits<double>::quiet_NaN());

static void
repair_adjacent_trim_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, int only_loop = -1,
	bool record_changes = true, bool skip_closing_join = false);


/* STEP permits a compact representation of a band on a singly-periodic
 * surface: the two closed boundary edges are separate one-trim loops and the
 * identified surface sides supply the implicit connection between them.
 * OpenNURBS requires that same topology to be cut into a Euclidean UV loop,
 * with two opposite uses of an explicit seam edge.  Insert that cut only when
 * exactly two candidate loops exist and the complete surface isocurve is
 * densely proven to agree on both periodic sides.  No source edge or surface
 * is approximated or moved. */
static size_t
repair_implicit_periodic_face_bands(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair !=
	    brlcad::step::RepairMode::Safe)
	return 0;

    size_t repaired = 0;
    const std::chrono::steady_clock::time_point pass_started =
	std::chrono::steady_clock::now();
    size_t periodic_faces_visited = 0;
    size_t closed_singletons_seen = 0;
    size_t periodic_singletons_seen = 0;
    size_t supplied_winding_candidates = 0;
    size_t arbitrary_split_attempts = 0;
    size_t arbitrary_split_successes = 0;
    size_t segmented_chain_scans = 0;
    long long arbitrary_split_microseconds = 0;
    size_t collapsed_boundary_attempts = 0;
    size_t collapsed_boundary_successes = 0;
    long long collapsed_boundary_microseconds = 0;
    size_t full_period_chain_attempts = 0;
    size_t full_period_chain_successes = 0;
    long long full_period_chain_microseconds = 0;
    std::map<std::pair<int, int>, double> proven_full_period_boundaries;
    std::set<int> aligned_band_surface_seams;
    /* A one-trim arbitrary full-period boundary becomes a two-trim chain
     * after split_periodic_boundary_at_native_seam().  Remember only those
     * proven chains so the general segmented-boundary solver does not scan
     * every ordinary two-edge loop in the model. */
    std::set<std::pair<int, int> > arbitrary_split_boundaries;
    const auto prove_collapsed_boundary = [brep, wrapper, entity_id,
	    &entity_type, &collapsed_boundary_attempts,
	    &collapsed_boundary_successes, &collapsed_boundary_microseconds](
	    ON_BrepLoop &loop, const ON_Surface *surface,
	    int closed_direction, double parameter_tolerance,
	    double *proven_open_parameter) {
	++collapsed_boundary_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool proven = regenerate_collapsed_periodic_boundary(brep, loop,
	    surface, closed_direction, parameter_tolerance, wrapper, entity_id,
	    entity_type, proven_open_parameter);
	collapsed_boundary_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (proven) ++collapsed_boundary_successes;
	return proven;
    };
    const auto regenerate_full_period_chain = [brep, wrapper, entity_id,
	    &entity_type, &full_period_chain_attempts,
	    &full_period_chain_successes, &full_period_chain_microseconds](
	    int loop_index, int closed_direction, double parameter_tolerance) {
	++full_period_chain_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool regenerated = regenerate_full_period_boundary_chain(brep,
	    loop_index, closed_direction, parameter_tolerance, wrapper, entity_id,
	    entity_type);
	full_period_chain_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (regenerated) ++full_period_chain_successes;
	return regenerated;
    };
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (brlcad::PullbackWorkCancelled())
	    break;
	ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface || (!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	++periodic_faces_visited;
	{
	    std::ostringstream detail;
	    detail << entity_type << " periodic-face=F" << fi;
	    if (face.m_face_user.i > 0)
		detail << "/shell#" << face.m_face_user.i;
	    detail << " loops=" << face.m_li.Count();
	    wrapper->SetProgressDetail("repairing implicit exact periodic face bands",
		entity_id, static_cast<uint64_t>(fi + 1),
		static_cast<uint64_t>(brep->m_F.Count()), "faces", detail.str());
	}
	int closed_direction = surface->IsClosed(0) ? 0 : 1;
	/* A toroidal face is closed in both parameter directions.  When its STEP
	 * bounds have already been split at a shared native seam, their complete
	 * pcurve chains prove which direction winds one full period.  Use that
	 * direction for the same exact band construction below.  Requiring two
	 * unambiguous winding witnesses avoids guessing which complementary patch
	 * a pair of arbitrary torus curves was intended to bound. */
	if (surface->IsClosed(0) && surface->IsClosed(1)) {
	    int winding_witnesses[2] = {0, 0};
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		ON_BrepLoop &loop = brep->m_L[li];
		if (loop.TrimCount() != 1) continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const double proof_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    const std::pair<int, int> proof_key(
			loop.m_loop_user.i, direction);
		    double proven_open_parameter = 0.0;
		    const std::map<std::pair<int, int>, double>::const_iterator
			proof = proven_full_period_boundaries.find(proof_key);
		    const bool proven = loop.m_loop_user.i > 0 &&
			proof != proven_full_period_boundaries.end();
		    if (proven)
			proven_open_parameter = proof->second;
		    if (proven || prove_collapsed_boundary(loop, surface,
			    direction, proof_tolerance, &proven_open_parameter)) {
			++winding_witnesses[direction];
			if (!proven && loop.m_loop_user.i > 0)
			    proven_full_period_boundaries[proof_key] =
				proven_open_parameter;
		    }
		}
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li >= 0 && li < brep->m_L.Count())
		    regenerate_split_periodic_boundary_chain(brep, brep->m_L[li],
			surface, wrapper, entity_id, entity_type);
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		const ON_BrepLoop &loop = brep->m_L[li];
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const ON_Interval other_domain = surface->Domain(1 - direction);
		    if (!domain.IsIncreasing() || !other_domain.IsIncreasing())
			continue;
		    const double direction_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    const double other_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, other_domain.Length()));
		    bool have_sample = false;
		    bool valid_chain = loop.TrimCount() > 0;
		    double first_closed = 0.0;
		    double previous_closed = 0.0;
		    double minimum_closed = DBL_MAX;
		    double maximum_closed = -DBL_MAX;
		    double first_other = 0.0;
		    double final_other = 0.0;
		    for (int lti = 0; valid_chain && lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *trim = loop.Trim(lti);
			const ON_Interval trim_domain = trim ? trim->Domain() :
			    ON_Interval::EmptyInterval;
			if (!trim || !trim_domain.IsIncreasing()) {
			    valid_chain = false;
			    break;
			}
			for (int sample = 0; sample <=
				kPcurveLocusScreeningSegments; ++sample) {
			    const ON_3dPoint point = trim->PointAt(
				trim_domain.ParameterAt(static_cast<double>(sample) /
				    kPcurveLocusScreeningSegments));
			    if (!point.IsValid() || !std::isfinite(point[direction]) ||
				    !std::isfinite(point[1 - direction])) {
				valid_chain = false;
				break;
			    }
			    double unwrapped = point[direction];
			    if (have_sample)
				unwrapped += round((previous_closed - unwrapped) /
				    domain.Length()) * domain.Length();
			    else {
				first_closed = unwrapped;
				first_other = point[1 - direction];
				have_sample = true;
			    }
			    previous_closed = unwrapped;
			    final_other = point[1 - direction];
			    minimum_closed = std::min(minimum_closed, unwrapped);
			    maximum_closed = std::max(maximum_closed, unwrapped);
			}
		    }
		    const bool full_winding = valid_chain && have_sample &&
			fabs(fabs(previous_closed - first_closed) -
			    domain.Length()) <= direction_tolerance &&
			fabs((maximum_closed - minimum_closed) -
			    domain.Length()) <= direction_tolerance &&
			fabs(final_other - first_other) <= other_tolerance;
		    if (full_winding)
			++winding_witnesses[direction];
		}
	    }
	    if (winding_witnesses[0] >= 2 &&
		    winding_witnesses[0] > winding_witnesses[1])
		closed_direction = 0;
	    else if (winding_witnesses[1] >= 2 &&
		    winding_witnesses[1] > winding_witnesses[0])
		closed_direction = 1;
	    else {
		if (wrapper->Verbose())
		{
		    std::cerr << entity_type << " #" << entity_id
			<< ": doubly periodic face F" << fi
			<< " lacked an unambiguous full-period boundary direction ("
			<< winding_witnesses[0] << '/' << winding_witnesses[1]
			<< " witnesses)";
		    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
			const int li = face.m_li[fli];
			if (li < 0 || li >= brep->m_L.Count()) continue;
			const ON_BrepLoop &loop = brep->m_L[li];
			const ON_BrepTrim *first = loop.Trim(0);
			const ON_BrepTrim *last = loop.Trim(loop.TrimCount() - 1);
			if (!first || !last) continue;
			const ON_3dPoint start = first->PointAtStart();
			const ON_3dPoint end = last->PointAtEnd();
			std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			    << "(trims=" << loop.TrimCount() << ',' << start.x << ':'
			    << start.y << "->" << end.x << ':' << end.y << ')';
		    }
		    std::cerr << std::endl;
		}
		continue;
	    }
	}
	const int open_direction = 1 - closed_direction;
	const ON_Interval closed_domain = surface->Domain(closed_direction);
	if (!closed_domain.IsIncreasing())
	    continue;
	const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, kPeriodicParameterSnapFraction *
	    std::max(1.0, closed_domain.Length()));
	/* A multi-loop spherical face is a cap only when its other bounds are
	 * contractible holes.  If another complete loop also winds once around
	 * the closed parameter direction, the two loops bound an annular band and
	 * must be paired by the band repair below.  Sample the supplied pcurves in
	 * loop order and unwrap only integral-period images.  This is deliberately
	 * a conservative exclusion proof: an uncertain sibling prevents a pole
	 * cut, but can never authorize one. */
	const auto loop_winds_full_period = [closed_direction, open_direction,
		&closed_domain, parameter_tolerance](const ON_BrepLoop &candidate) {
	    if (candidate.TrimCount() < 1)
		return false;
	    bool have_sample = false;
	    double first_closed = 0.0;
	    double previous_closed = 0.0;
	    double minimum_closed = 0.0;
	    double maximum_closed = 0.0;
	    double first_open = 0.0;
	    double previous_open = 0.0;
	    const double period = closed_domain.Length();
	    for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		const ON_BrepTrim *candidate_trim = candidate.Trim(lti);
		if (!candidate_trim || !candidate_trim->Domain().IsIncreasing())
		    return false;
		const ON_Interval trim_domain = candidate_trim->Domain();
		for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
			sample++) {
		    const ON_3dPoint point = candidate_trim->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) /
			kPcurveLocusScreeningSegments));
		    if (!point.IsValid() ||
			    !std::isfinite(point[closed_direction]) ||
			    !std::isfinite(point[open_direction]))
			return false;
		    double unwrapped = point[closed_direction];
		    if (have_sample) {
			unwrapped += round((previous_closed - unwrapped) /
			    period) * period;
		    } else {
			first_closed = unwrapped;
			minimum_closed = unwrapped;
			maximum_closed = unwrapped;
			first_open = point[open_direction];
			have_sample = true;
		    }
		    previous_closed = unwrapped;
		    previous_open = point[open_direction];
		    minimum_closed = std::min(minimum_closed, unwrapped);
		    maximum_closed = std::max(maximum_closed, unwrapped);
		}
	    }
	    if (!have_sample)
		return false;
	    const double net_travel = previous_closed - first_closed;
	    const double parameter_span = maximum_closed - minimum_closed;
	    const double open_tolerance = std::max(ON_ZERO_TOLERANCE *
		kNumericalToleranceScale, kPeriodicParameterSnapFraction *
		std::max(1.0, fabs(first_open)));
	    return fabs(fabs(net_travel) - period) <= parameter_tolerance &&
		fabs(parameter_span - period) <= parameter_tolerance &&
		fabs(previous_open - first_open) <= open_tolerance;
	};
	const auto exact_edge_chain_winding_seam = [brep, surface,
		closed_direction, open_direction, &closed_domain,
		parameter_tolerance](const ON_BrepLoop &candidate,
		double *seam_parameter) {
	    if (!seam_parameter || candidate.TrimCount() < 2)
		return false;
	    const double period = closed_domain.Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		return false;
	    const bool surface_closed[2] = {
		surface->IsClosed(0), surface->IsClosed(1)
	    };
	    const ON_Interval surface_domains[2] = {
		surface->Domain(0), surface->Domain(1)
	    };
	    brlcad::PullbackContext context;
	    bool have_sample = false;
	    double first_closed = 0.0;
	    double previous_closed = 0.0;
	    double minimum_closed = DBL_MAX;
	    double maximum_closed = -DBL_MAX;
	    double first_open = 0.0;
	    double previous_open = 0.0;
	    for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = candidate.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || !edge->Domain().IsIncreasing() ||
			!trim->Domain().IsIncreasing())
		    return false;
		/* This proof authorizes a topology-preserving surface seam move.
		 * Require the immutable edge to lie on the surface within the model's
		 * asserted uncertainty; previously enlarged OpenNURBS tolerances are
		 * deliberately not sufficient evidence for changing parameterization. */
		const double proof_tolerance = std::max(LocalUnits::tolerance,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		const double solver_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    proof_tolerance * 0.1);
		const int samples = std::min(256,
		    std::max(64, edge->SpanCount() * 4));
		for (int sample = 0; sample <= samples; ++sample) {
		    if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) / samples;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim->m_bRev3d ?
			    1.0 - fraction : fraction));
		    const ON_3dPoint supplied = trim->PointAt(
			trim->Domain().ParameterAt(fraction));
		    ON_2dPoint seed(supplied.x, supplied.y);
		    if (have_sample) {
			seed[closed_direction] = previous_closed;
			seed[open_direction] = previous_open;
		    }
		    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint lift;
		    double distance = DBL_MAX;
		    bool projected = edge_point.IsValid() && seed.IsValid() &&
			context.SurfaceClosestPointFromSeed(surface, edge_point,
			    seed, uv, lift, distance, proof_tolerance,
			    surface_closed, surface_domains, solver_tolerance) &&
			distance <= proof_tolerance;
		    if (!projected)
			projected = edge_point.IsValid() &&
			    context.SurfaceClosestPoint(surface, edge_point, uv,
				lift, distance, 0, solver_tolerance,
				proof_tolerance) &&
			    distance <= proof_tolerance;
		    if (!projected)
			return false;
		    double closed = uv[closed_direction];
		    double open = uv[open_direction];
		    if (have_sample) {
			closed += round((previous_closed - closed) / period) * period;
			if (surface->IsClosed(open_direction)) {
			    const double open_period =
				surface_domains[open_direction].Length();
			    if (open_period > ON_ZERO_TOLERANCE)
				open += round((previous_open - open) /
				    open_period) * open_period;
			}
		    } else {
			first_closed = closed;
			first_open = open;
			have_sample = true;
		    }
		    previous_closed = closed;
		    previous_open = open;
		    minimum_closed = std::min(minimum_closed, closed);
		    maximum_closed = std::max(maximum_closed, closed);
		}
	    }
	    if (!have_sample)
		return false;
	    double open_closure = previous_open - first_open;
	    if (surface->IsClosed(open_direction)) {
		const double open_period = surface_domains[open_direction].Length();
		if (open_period > ON_ZERO_TOLERANCE)
		    open_closure -= round(open_closure / open_period) * open_period;
	    }
	    const double open_tolerance = std::max(parameter_tolerance,
		kPeriodicParameterSnapFraction * std::max(1.0,
		    surface_domains[open_direction].Length()));
	    if (fabs(fabs(previous_closed - first_closed) - period) >
		    parameter_tolerance ||
		fabs((maximum_closed - minimum_closed) - period) >
		    parameter_tolerance ||
		fabs(open_closure) > open_tolerance)
		return false;
	    double native_seam = first_closed -
		floor((first_closed - closed_domain.Min()) / period) * period;
	    if (native_seam > closed_domain.Max())
		native_seam -= period;
	    if (native_seam < closed_domain.Min())
		native_seam += period;
	    *seam_parameter = native_seam;
	    return true;
	};

	/* STEP can bound a spherical cap intrinsically with two or more ordinary
	 * arcs which together wind once around the sphere.  OpenNURBS requires the
	 * equivalent rectangular parameter-space loop: the physical boundary,
	 * two opposite uses of an exact meridian seam, and a singular pole trim.
	 * Normalize the complete edge chain to one native period first, then use
	 * the existing densely validated pole-cut construction.  Requiring one
	 * face loop and a proven full-period chain avoids turning an ordinary inner
	 * loop or a partial latitude arc into an artificial pole cut. */
    if (face.m_li.Count() == 1) {
	const int only_loop = face.m_li[0];
	bool have_parameter = false;
	double first_unwrapped = 0.0;
	double previous_unwrapped = 0.0;
	if (only_loop >= 0 && only_loop < brep->m_L.Count()) {
	    const ON_BrepLoop &loop = brep->m_L[only_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
		if (!curve || !curve->Domain().IsIncreasing()) {
		    have_parameter = false;
		    break;
		}
		const ON_Interval curve_domain = curve->Domain();
		for (int sample = 0;
			sample <= kBoundaryParameterSearchSegments; ++sample) {
		    if (brlcad::PullbackWorkCancelled()) {
			have_parameter = false;
			break;
		    }
		    const ON_3dPoint point = curve->PointAt(
			curve_domain.ParameterAt(static_cast<double>(sample) /
			kBoundaryParameterSearchSegments));
		    if (!point.IsValid() ||
			    !std::isfinite(point[closed_direction])) {
			have_parameter = false;
			break;
		    }
		    double unwrapped = point[closed_direction];
		    if (have_parameter)
			unwrapped += round((previous_unwrapped - unwrapped) /
			    closed_domain.Length()) * closed_domain.Length();
		    else {
			first_unwrapped = unwrapped;
			have_parameter = true;
		    }
		    previous_unwrapped = unwrapped;
		}
		if (!have_parameter)
		    break;
	    }
	}
	const double net_winding = have_parameter ?
	    previous_unwrapped - first_unwrapped : 0.0;
	const bool full_period_chain = have_parameter &&
	    fabs(fabs(net_winding) - closed_domain.Length()) <=
	    parameter_tolerance;
	/* A one-loop face which merely crosses the native seam is an ordinary
	 * bounded patch, not an implicit spherical cap.  Requiring one full
	 * pcurve winding prevents the exact-edge regeneration below from spending
	 * the item budget trying to force a contractible loop around the surface.
	 * Multi-loop periodic bands retain their independent exact-edge proof,
	 * since their supplied pcurves may be the malformed data being repaired. */
	if (!full_period_chain && wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": one-loop full-period boundary candidate L" << only_loop
		<< " rejected because net pcurve winding " << net_winding
		<< " does not equal one period " << closed_domain.Length()
		<< std::endl;
	if (full_period_chain && regenerate_full_period_chain(only_loop,
		closed_direction, parameter_tolerance)) {
		ON_BrepLoop &normalized_loop = brep->m_L[only_loop];
		ON_BrepTrim *first = normalized_loop.Trim(0);
		ON_BrepTrim *last = normalized_loop.Trim(
		    normalized_loop.TrimCount() - 1);
		const ON_BrepFace *normalized_face = normalized_loop.Face();
		const ON_Surface *normalized_surface = normalized_face ?
		    normalized_face->SurfaceOf() : NULL;
		double topology_tolerance = LocalUnits::tolerance;
		for (int lti = 0; lti < normalized_loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = normalized_loop.Trim(lti);
		    if (!trim) continue;
		    topology_tolerance = std::max(topology_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		    if (trim->Edge())
			topology_tolerance = std::max(topology_tolerance,
			    trim->Edge()->m_tolerance);
		}
		const ON_3dPoint boundary_end_3d = last ?
		    last->PointAtEnd() : ON_3dPoint::UnsetPoint;
		const ON_3dPoint boundary_start_3d = first ?
		    first->PointAtStart() : ON_3dPoint::UnsetPoint;
		if (first && last && normalized_surface &&
			step_insert_periodic_pole_cut(brep, normalized_loop,
			normalized_surface, *last,
			ON_2dPoint(boundary_end_3d.x, boundary_end_3d.y),
			ON_2dPoint(boundary_start_3d.x, boundary_start_3d.y),
			topology_tolerance)) {
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"inserted an exact pole cut for a multi-edge full-period boundary");
		    ++repaired;
		    continue;
		}
		/* regenerate_full_period_boundary_chain installs a deep BREP copy,
		 * invalidating the face/surface references captured at the top of this
		 * iteration.  Even if the pole proof is rejected, leave the exact
		 * normalized boundary for later repair stages and advance without using
		 * those stale references. */
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": normalized multi-edge full-period loop L"
			<< only_loop
			<< " but no exact surface-pole cut was proven" << std::endl;
		continue;
	    }
	    /* A complete intrinsic boundary need not begin at the native seam.
	     * Materialize a translated seam crossing inside an open STEP edge,
	     * then restart with the compacted edge-use graph.  The next pass can
	     * rotate the chain at that proven seam vertex and insert the pole cut. */
	    if (split_open_periodic_boundary_crossing(brep, face, only_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type)) {
		--fi;
		continue;
	    }
	}

	struct BoundaryCandidate {
	    int loop_index;
	    std::vector<int> trim_indices;
	    std::vector<int> period_shifts;
	    int vertex_index;
	    double open_parameter;
	    int open_period_shift;
	};
	std::vector<BoundaryCandidate> candidates;
	const auto proven_chain_join = [brep, surface, closed_direction,
		&closed_domain](const ON_BrepTrim *previous,
		const ON_BrepTrim *next, double previous_closed,
		double previous_open, double next_closed, double next_open) {
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count())
		return false;
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge())
		tolerance = std::max(tolerance, previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		std::max(previous->m_tolerance[0], previous->m_tolerance[1]));
	    tolerance = std::max(tolerance,
		std::max(next->m_tolerance[0], next->m_tolerance[1]));
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const double period = closed_domain.Length();
	    const auto native_parameter = [&closed_domain, period](double value) {
		value -= floor((value - closed_domain.Min()) / period) * period;
		if (value > closed_domain.Max()) value -= period;
		if (value < closed_domain.Min()) value += period;
		return std::max(closed_domain.Min(),
		    std::min(closed_domain.Max(), value));
	    };
	    ON_3dPoint first;
	    ON_3dPoint second;
	    first[closed_direction] = native_parameter(previous_closed);
	    first[1 - closed_direction] = previous_open;
	    first.z = 0.0;
	    second[closed_direction] = native_parameter(next_closed);
	    second[1 - closed_direction] = next_open;
	    second.z = 0.0;
	    /* Compare on one adjacent periodic image before taking the midpoint;
	     * averaging opposite native boundaries would test the antipode. */
	    second[closed_direction] += round((first[closed_direction] -
		second[closed_direction]) / period) * period;
	    ON_3dPoint middle = 0.5 * (first + second);
	    first[closed_direction] = native_parameter(first[closed_direction]);
	    second[closed_direction] = native_parameter(second[closed_direction]);
	    middle[closed_direction] = native_parameter(middle[closed_direction]);
	    const ON_3dPoint first_lift = surface->PointAt(first.x, first.y);
	    const ON_3dPoint second_lift = surface->PointAt(second.x, second.y);
	    const ON_3dPoint middle_lift = surface->PointAt(middle.x, middle.y);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    return first_lift.IsValid() && second_lift.IsValid() &&
		middle_lift.IsValid() &&
		first_lift.DistanceTo(vertex) <= tolerance &&
		second_lift.DistanceTo(vertex) <= tolerance &&
		middle_lift.DistanceTo(vertex) <= tolerance;
	};
	bool topology_split = false;
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		continue;
	    ON_BrepLoop &loop = brep->m_L[li];
	    if (loop.TrimCount() < 1)
		continue;
	    /* A doubly-periodic boundary may already have been split in the
	     * surface's other closed direction by an adjacent face use.  Once the
	     * complete chain proves one winding in this face's selected direction,
	     * materialize its interior native-seam crossing as well.  Without that
	     * second exact cut the chain remains intrinsically closed (its first and
	     * last UV endpoints differ by one period), so it cannot participate in
	     * the Euclidean OpenNURBS band loop below.  The splitter changes every
	     * use of the affected 3-D edge and independently validates the crossing;
	     * if no open edge contains the native seam, it is a no-op. */
	    if (loop.TrimCount() > 1 && loop_winds_full_period(loop) &&
		    split_open_periodic_boundary_crossing(brep, face, li,
			closed_direction, parameter_tolerance, wrapper, entity_id,
			entity_type)) {
		topology_split = true;
		break;
	    }
	    ON_BrepTrim *trim = loop.Trim(0);
	    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    bool periodic_closure = loop.TrimCount() == 1 && trim && edge &&
		periodic_loop_closure(brep, &loop, trim, trim,
		    parameter_tolerance);
	    const std::pair<int, int> proof_key(loop.m_loop_user.i,
		closed_direction);
	    double proven_open_parameter = trim ?
		trim->PointAtStart()[open_direction] : 0.0;
	    const std::map<std::pair<int, int>, double>::const_iterator proof =
		proven_full_period_boundaries.find(proof_key);
	    bool full_period_proven = loop.m_loop_user.i > 0 &&
		proof != proven_full_period_boundaries.end();
	    if (full_period_proven)
		proven_open_parameter = proof->second;
	    if (loop.TrimCount() == 1 && trim && edge &&
		    (full_period_proven || prove_collapsed_boundary(loop, surface,
			 closed_direction, parameter_tolerance,
			 &proven_open_parameter))) {
		if (!full_period_proven && loop.m_loop_user.i > 0)
		    proven_full_period_boundaries[proof_key] =
			proven_open_parameter;
		ON_3dPoint proven_min_uv;
		proven_min_uv[closed_direction] = closed_domain.Min();
		proven_min_uv[open_direction] = proven_open_parameter;
		proven_min_uv.z = 0.0;
		ON_3dPoint proven_max_uv = proven_min_uv;
		proven_max_uv[closed_direction] = closed_domain.Max();
		const ON_3dPoint proven_min_lift = surface->PointAt(
		    proven_min_uv.x, proven_min_uv.y);
		const ON_3dPoint proven_max_lift = surface->PointAt(
		    proven_max_uv.x, proven_max_uv.y);
		const ON_3dPoint &vertex = brep->m_V[trim->m_vi[0]].point;
		const double topology_tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		const bool proven_vertex_on_native_seam =
		    proven_min_lift.IsValid() && proven_max_lift.IsValid() &&
		    proven_min_lift.DistanceTo(vertex) <= topology_tolerance &&
		    proven_max_lift.DistanceTo(vertex) <= topology_tolerance;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": proven periodic boundary F" << fi << "/L" << li
			<< " direction=" << closed_direction << " open="
			<< proven_open_parameter << " native-vertex="
			<< (proven_vertex_on_native_seam ? "yes" : "no")
			<< " distances=" << proven_min_lift.DistanceTo(vertex)
			<< '/' << proven_max_lift.DistanceTo(vertex)
			<< " tolerance=" << topology_tolerance << std::endl;
		if (!proven_vertex_on_native_seam &&
			split_periodic_boundary_at_native_seam(brep, li,
			    trim->m_trim_index, surface, closed_direction,
			    proven_open_parameter, wrapper, entity_id,
			    entity_type)) {
		    topology_split = true;
		    break;
		}
		if (proven_vertex_on_native_seam &&
			regenerate_native_seam_periodic_boundary(brep, loop,
			    surface, closed_direction, wrapper, entity_id,
			    entity_type, proven_open_parameter)) {
		    trim = loop.Trim(0);
		    edge = trim ? trim->Edge() : NULL;
		    periodic_closure = trim && edge && periodic_loop_closure(
			brep, &loop, trim, trim, parameter_tolerance);
		if (trim && periodic_closure) {
			const ON_3dPoint boundary_end_3d = trim->PointAtEnd();
			const ON_3dPoint boundary_start_3d = trim->PointAtStart();
			const ON_2dPoint boundary_end(boundary_end_3d.x,
			    boundary_end_3d.y);
			const ON_2dPoint boundary_start(boundary_start_3d.x,
			    boundary_start_3d.y);
			bool competing_full_period_boundary = false;
			if (face.m_li.Count() == 2 &&
				loop.m_type == ON_BrepLoop::outer) {
			    for (int sibling_index = 0;
				    sibling_index < face.m_li.Count(); ++sibling_index) {
				const int sibling_li = face.m_li[sibling_index];
				if (sibling_li == li || sibling_li < 0 ||
					sibling_li >= brep->m_L.Count())
				    continue;
				const ON_BrepLoop &sibling = brep->m_L[sibling_li];
				if (loop_winds_full_period(sibling)) {
				    competing_full_period_boundary = true;
				    break;
				}
			    }
			}
			/* A full-period outer boundary plus only contractible inner
			 * loops is a cap with holes.  A second winding boundary is an
			 * annular band and needs two paired seam uses instead. */
		    std::string pole_cut_failure;
		    if ((face.m_li.Count() == 1 ||
			    (loop.m_type == ON_BrepLoop::outer &&
			     !competing_full_period_boundary)) &&
			step_insert_periodic_pole_cut(brep, loop, surface,
			*trim, boundary_end, boundary_start,
			topology_tolerance, &pole_cut_failure)) {
			    wrapper->RecordRepair(entity_id, entity_type,
				"edge_loop",
				"inserted an exact paired seam cut for a proven full-period boundary and surface pole");
			    topology_split = true;
			    break;
			}
		    if (wrapper->Verbose() && !pole_cut_failure.empty())
			std::cerr << entity_type << " #" << entity_id
			    << ": periodic pole cut F" << fi << "/L" << li
			    << " rejected: " << pole_cut_failure << std::endl;
		    }
		}
	    }
	    /* A full-period boundary need not be an isocurve.  STEP also permits a
	     * single closed spline which winds once around a periodic surface while
	     * varying in the other parameter direction.  Its endpoint topology and
	     * pcurve establish the candidate cut; the splitter independently
	     * projects the complete immutable 3-D edge, proves exactly one winding,
	     * and densely validates both resulting edge/trim fragments before any
	     * topology is committed. */
	    const bool fixed_full_period_proved = full_period_proven ||
		(loop.m_loop_user.i > 0 && proven_full_period_boundaries.find(
		    proof_key) != proven_full_period_boundaries.end());
	    const ON_3dPoint supplied_start = trim ? trim->PointAtStart() :
		ON_3dPoint::UnsetPoint;
	    const ON_3dPoint supplied_end = trim ? trim->PointAtEnd() :
		ON_3dPoint::UnsetPoint;
	    const bool supplied_periodic_winding = supplied_start.IsValid() &&
		supplied_end.IsValid() &&
		fabs(fabs(supplied_end[closed_direction] -
		    supplied_start[closed_direction]) - closed_domain.Length()) <=
		    parameter_tolerance &&
		fabs(supplied_end[open_direction] -
		    supplied_start[open_direction]) <= parameter_tolerance;
	    const bool closed_singleton = loop.TrimCount() == 1 && trim && edge &&
		    !fixed_full_period_proved &&
		    edge->m_vi[0] == edge->m_vi[1] &&
		    trim->m_vi[0] == trim->m_vi[1];
	    if (closed_singleton) ++closed_singletons_seen;
	    if (closed_singleton && periodic_closure) ++periodic_singletons_seen;
	    if (closed_singleton && periodic_closure && supplied_periodic_winding)
		++supplied_winding_candidates;
	    bool arbitrary_split = false;
	    if (closed_singleton && periodic_closure && supplied_periodic_winding) {
		++arbitrary_split_attempts;
		const std::chrono::steady_clock::time_point split_started =
		    std::chrono::steady_clock::now();
		arbitrary_split = split_periodic_boundary_at_native_seam(brep, li,
			trim->m_trim_index, surface, closed_direction,
			std::numeric_limits<double>::quiet_NaN(), wrapper,
			entity_id, entity_type);
		arbitrary_split_microseconds +=
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - split_started).count();
	    }
	    if (arbitrary_split) {
		++arbitrary_split_successes;
		arbitrary_split_boundaries.insert(proof_key);
		topology_split = true;
		break;
	    }
	    if (wrapper->Verbose() && trim && edge &&
		    edge->m_vi[0] == edge->m_vi[1]) {
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		std::cerr << entity_type << " #" << entity_id
		    << ": implicit periodic singleton F" << fi << "/L" << li
		    << "/T" << trim->m_trim_index << " uv=" << start.x << ':'
		    << start.y << "->" << end.x << ':' << end.y
		    << " trim-vertices=" << trim->m_vi[0] << '/'
		    << trim->m_vi[1] << " edge-vertex=" << edge->m_vi[0]
		    << " periodic-closure=" << (periodic_closure ? "yes" : "no")
		    << std::endl;
	    }
	    if (loop.TrimCount() == 1) {
		const bool proven_periodic_closure = periodic_closure ||
		    fixed_full_period_proved;
		if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
			trim->m_vi[0] != trim->m_vi[1] ||
			trim->m_vi[0] != edge->m_vi[0] || !trim->TrimCurveOf() ||
			!proven_periodic_closure)
		    continue;
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		if (!start.IsValid() || !end.IsValid() ||
			fabs(start[open_direction] - end[open_direction]) >
			    parameter_tolerance)
		    continue;
		BoundaryCandidate candidate;
		candidate.loop_index = li;
		candidate.trim_indices.push_back(trim->m_trim_index);
		/* Normalize a complete singleton winding onto the native surface
		 * interval before the two physical boundaries are oriented as the
		 * lower and upper sides of the band.  A decreasing exact boundary is
		 * commonly supplied as 0 -> -period rather than max -> min.  Those
		 * parameterizations have identical lifts, but treating the former as
		 * an unshifted candidate makes the later native-side check reject an
		 * otherwise exact band and leaves a one-trim loop open in Euclidean
		 * UV.  Record only the integral-period image implied by the proven
		 * winding; install_period_shifts() independently validates every
		 * translated sample before committing it. */
		const double period = closed_domain.Length();
		const double winding = end[closed_direction] -
		    start[closed_direction];
		const double native_start = winding >= 0.0 ?
		    closed_domain.Min() : closed_domain.Max();
		candidate.period_shifts.push_back(static_cast<int>(llround(
		    (native_start - start[closed_direction]) / period)));
		candidate.vertex_index = trim->m_vi[0];
		candidate.open_parameter = 0.5 *
		    (start[open_direction] + end[open_direction]);
		candidate.open_period_shift = 0;
		candidates.push_back(candidate);
		continue;
	    }
	    ON_BrepTrim *first_trim = loop.Trim(0);
	    ON_BrepTrim *second_trim = loop.Trim(1);
	    const bool proven_segmented_full_period = loop.TrimCount() > 1 &&
		loop_winds_full_period(loop);
	    if (loop.TrimCount() > 2 || proven_segmented_full_period ||
		    arbitrary_split_boundaries.find(proof_key) !=
			arbitrary_split_boundaries.end()) {
		++segmented_chain_scans;
		/* A periodic band boundary need not be a single circle.  AP214
		 * models commonly use a scalloped or segmented inner boundary whose
		 * intermediate pcurve joins are continuous, with only the final-to-
		 * first join separated by one native period.  Preserve the complete
		 * STEP chain as one candidate; the band construction below will join
		 * it to the other physical boundary with the one exact shared seam
		 * OpenNURBS requires.  Two-edge chains enter this general path only
		 * after loop_winds_full_period() has proved their complete winding, so
		 * ordinary two-edge face loops retain the cheaper specialized test. */
		/* The split point can occur anywhere in the cyclic STEP edge order.
		 * Rotate the chain to that proven native-seam vertex, then translate
		 * each complete pcurve by an integral period so the same exact lifts
		 * form one Euclidean path from the native minimum to maximum (or the
		 * reverse).  No arbitrary seam angle or topology is inferred. */
		const double period = closed_domain.Length();
		bool added = false;
		for (int cut = 0; !added && cut < loop.TrimCount(); ++cut) {
		    ON_BrepTrim *before = loop.Trim(cut);
		    ON_BrepTrim *after = loop.Trim((cut + 1) % loop.TrimCount());
		    if (!before || !after || !before->TrimCurveOf() ||
			    !after->TrimCurveOf() ||
			    before->m_vi[1] != after->m_vi[0])
			continue;
		    const ON_3dPoint cut_end = before->PointAtEnd();
		    const ON_3dPoint cut_start = after->PointAtStart();
		    if (!cut_end.IsValid() || !cut_start.IsValid() ||
			    fabs(cut_end[open_direction] -
				cut_start[open_direction]) > parameter_tolerance)
			continue;
		    const double cut_closed_gap = fabs(
			cut_end[closed_direction] - cut_start[closed_direction]);
		    if (cut_closed_gap > parameter_tolerance &&
			    fabs(cut_closed_gap - period) > parameter_tolerance)
			continue;
		    const double native_image = closed_domain.Min() +
			round((cut_start[closed_direction] - closed_domain.Min()) /
			    period) * period;
		    if (fabs(cut_start[closed_direction] - native_image) >
			    parameter_tolerance)
			continue;

		    for (int direction = 0; !added && direction < 2; ++direction) {
			const bool increasing = direction == 0;
			const double target_start = increasing ?
			    closed_domain.Min() : closed_domain.Max();
			const double target_end = increasing ?
			    closed_domain.Max() : closed_domain.Min();
			std::vector<int> chain_trim_indices;
			std::vector<int> chain_period_shifts;
			double expected_closed = target_start;
			double expected_open = cut_start[open_direction];
			ON_BrepTrim *previous_trim = NULL;
			bool continuous_chain = true;
			for (int offset = 0; continuous_chain &&
				offset < loop.TrimCount(); ++offset) {
			    ON_BrepTrim *current = loop.Trim(
				(cut + 1 + offset) % loop.TrimCount());
			    if (!current || !current->TrimCurveOf()) {
				continuous_chain = false;
				break;
			    }
			    const ON_3dPoint start = current->PointAtStart();
			    const ON_3dPoint end = current->PointAtEnd();
			    if (!start.IsValid() || !end.IsValid()) {
				continuous_chain = false;
				break;
			    }
			    const int shift = static_cast<int>(llround(
				(expected_closed - start[closed_direction]) / period));
			    const double shifted_start = start[closed_direction] +
				shift * period;
			    const bool exact_join = fabs(shifted_start -
				expected_closed) <= parameter_tolerance &&
				fabs(start[open_direction] - expected_open) <=
				    parameter_tolerance;
			    if (!exact_join && (!previous_trim ||
				    !proven_chain_join(previous_trim, current,
					expected_closed, expected_open, shifted_start,
					start[open_direction]))) {
				continuous_chain = false;
				break;
			    }
			    chain_trim_indices.push_back(current->m_trim_index);
			    chain_period_shifts.push_back(shift);
			    expected_closed = end[closed_direction] + shift * period;
			    expected_open = end[open_direction];
			    previous_trim = current;
			}
			if (!continuous_chain || chain_trim_indices.empty() ||
				fabs(expected_closed - target_end) >
				    parameter_tolerance ||
				fabs(expected_open - cut_start[open_direction]) >
				    parameter_tolerance)
			    continue;
			ON_BrepTrim *chain_first = brep->m_T.At(
			    chain_trim_indices.front());
			ON_BrepTrim *chain_last = brep->m_T.At(
			    chain_trim_indices.back());
			if (!chain_first || !chain_last ||
				chain_first->m_vi[0] != chain_last->m_vi[1])
			    continue;
			BoundaryCandidate candidate;
			candidate.loop_index = li;
			candidate.trim_indices.swap(chain_trim_indices);
			candidate.period_shifts.swap(chain_period_shifts);
			candidate.vertex_index = chain_first->m_vi[0];
			candidate.open_parameter = 0.5 *
			    (cut_start[open_direction] + expected_open);
			candidate.open_period_shift = 0;
			candidates.push_back(candidate);
			added = true;
		    }
		}
		if (added)
		    continue;
		if (loop.TrimCount() > 2)
		    continue;
	    }
	    if (!first_trim || !second_trim || !first_trim->TrimCurveOf() ||
		    !second_trim->TrimCurveOf() ||
		    first_trim->m_vi[1] != second_trim->m_vi[0] ||
		    second_trim->m_vi[1] != first_trim->m_vi[0])
		continue;
	    const ON_3dPoint first_start = first_trim->PointAtStart();
	    const ON_3dPoint first_end = first_trim->PointAtEnd();
	    const ON_3dPoint second_start = second_trim->PointAtStart();
	    const ON_3dPoint second_end = second_trim->PointAtEnd();
	    const double chain_winding = second_end[closed_direction] -
		first_start[closed_direction];
	    const double native_chain_start = chain_winding >= 0.0 ?
		closed_domain.Min() : closed_domain.Max();
	    const int chain_period_shift = static_cast<int>(llround(
		(native_chain_start - first_start[closed_direction]) /
		closed_domain.Length()));
	    const double shifted_first_start = first_start[closed_direction] +
		chain_period_shift * closed_domain.Length();
	    const double shifted_second_end = second_end[closed_direction] +
		chain_period_shift * closed_domain.Length();
	    const bool increasing_periodic_chain =
		fabs(shifted_first_start - closed_domain.Min()) <=
		    parameter_tolerance &&
		fabs(shifted_second_end - closed_domain.Max()) <=
		    parameter_tolerance;
	    const bool decreasing_periodic_chain =
		fabs(shifted_first_start - closed_domain.Max()) <=
		    parameter_tolerance &&
		fabs(shifted_second_end - closed_domain.Min()) <=
		    parameter_tolerance;
	    if (!first_start.IsValid() || !first_end.IsValid() ||
		    !second_start.IsValid() || !second_end.IsValid() ||
		    first_end.DistanceTo(second_start) > parameter_tolerance ||
		    (!increasing_periodic_chain &&
		     !decreasing_periodic_chain) ||
		    fabs(first_start[open_direction] - first_end[open_direction]) >
			parameter_tolerance ||
		    fabs(first_start[open_direction] - second_start[open_direction]) >
			parameter_tolerance ||
		    fabs(first_start[open_direction] - second_end[open_direction]) >
			parameter_tolerance)
		continue;
	    BoundaryCandidate candidate;
	    candidate.loop_index = li;
	    candidate.trim_indices.push_back(first_trim->m_trim_index);
	    candidate.trim_indices.push_back(second_trim->m_trim_index);
	    /* Keep both pieces on the same exact periodic image.  In particular,
	     * a supplied 0 -> -period chain must become max -> min as a unit;
	     * independently normalizing its pieces would destroy their valid
	     * interior join.  install_period_shifts() densely proves that this
	     * integral translation preserves both surface lifts. */
	    candidate.period_shifts.push_back(chain_period_shift);
	    candidate.period_shifts.push_back(chain_period_shift);
	    candidate.vertex_index = first_trim->m_vi[0];
	    candidate.open_parameter = first_start[open_direction];
	    candidate.open_period_shift = 0;
	    candidates.push_back(candidate);
	}
	/* A supplied multi-edge boundary can cross the native parameter seam at
	 * an edge endpoint while its pcurve itself remains on the principal
	 * branch.  Whole-curve translations cannot unwrap that final edge.  Once
	 * the other physical boundary proves this is a periodic face band, rebuild
	 * the missing winding directly from the exact 3-D STEP edge chain.  The
	 * helper accepts only a native-seam topology vertex and densely validates
	 * every regenerated edge lift before changing the BREP. */
	const int other_loop = face.m_li.Count() == 2 && candidates.size() == 1 ?
	    (face.m_li[0] == candidates[0].loop_index ?
		face.m_li[1] : face.m_li[0]) : -1;
	/* If the exact boundary winds once but no existing topology vertex lies
	 * on the private OpenNURBS seam, prefer changing that private
	 * parameterization over splitting a shared STEP edge and every adjacent
	 * face use.  The complete immutable edge chain independently proves the
	 * winding and supplies the requested seam phase.  The relocation helper
	 * regenerates every pcurve on the affected surface transactionally and
	 * densely validates its lift before this face is reconsidered. */
	if (!topology_split && other_loop >= 0 &&
		other_loop < brep->m_L.Count() && face.m_si >= 0 &&
		aligned_band_surface_seams.find(face.m_si) ==
		    aligned_band_surface_seams.end()) {
	    double topology_seam = std::numeric_limits<double>::quiet_NaN();
	    if (exact_edge_chain_winding_seam(brep->m_L[other_loop],
		    &topology_seam)) {
		std::string relocation_failure;
		if (relocate_closed_surface_loop_seam(brep, other_loop,
			closed_direction, LocalUnits::tolerance, wrapper,
			entity_id, entity_type, &relocation_failure,
			topology_seam)) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type,
			"trim_pcurve",
			"aligned a closed surface seam with a proven periodic boundary topology vertex");
		    --fi;
		    continue;
		}
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic band surface-seam alignment F" << fi
			<< "/L" << other_loop << " rejected: "
			<< relocation_failure << std::endl;
	    }
	    aligned_band_surface_seams.insert(face.m_si);
	}
	if (!topology_split && other_loop >= 0) {
	    if (regenerate_full_period_chain(other_loop, closed_direction,
		    parameter_tolerance))
		topology_split = true;
	}
	/* Endpoint normalization can turn a valid arbitrary full-period pcurve
	 * into a closed native-domain image before this pass runs.  In that form
	 * the pcurve no longer proves its winding, even though its immutable closed
	 * 3-D STEP edge still does.  A sibling candidate already proves that this
	 * two-loop face is a periodic band.  Let the native-seam splitter project
	 * the other exact edge and independently require one complete winding
	 * before it changes topology.  This recovers the proof without treating a
	 * merely contractible closed curve as a band boundary. */
	if (!topology_split && other_loop >= 0 &&
		other_loop < brep->m_L.Count()) {
	    ON_BrepLoop &collapsed_loop = brep->m_L[other_loop];
	    ON_BrepTrim *collapsed_trim = collapsed_loop.TrimCount() == 1 ?
		collapsed_loop.Trim(0) : NULL;
	    const ON_BrepEdge *collapsed_edge = collapsed_trim ?
		collapsed_trim->Edge() : NULL;
	    if (collapsed_trim && collapsed_edge &&
		    collapsed_trim->m_vi[0] == collapsed_trim->m_vi[1] &&
		    collapsed_edge->m_vi[0] == collapsed_edge->m_vi[1] &&
		    split_periodic_boundary_at_native_seam(brep, other_loop,
			collapsed_trim->m_trim_index, surface, closed_direction,
			std::numeric_limits<double>::quiet_NaN(), wrapper,
			entity_id, entity_type))
		topology_split = true;
	}
	/* One boundary may already be a native full-period candidate while the
	 * other crosses the same private surface seam inside an open STEP edge.
	 * Materialize that exact crossing as a shared topology vertex, then restart
	 * this face with the compacted edge-use graph. */
	if (!topology_split && other_loop >= 0 &&
		split_open_periodic_boundary_crossing(brep, face, other_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type))
	    topology_split = true;
	if (topology_split) {
	    --fi;
	    continue;
	}
	const auto report_rejection = [brep, wrapper, entity_id, &entity_type, fi,
		&face, &candidates](const char *reason) {
	    if (!wrapper->Verbose() || candidates.empty())
		return;
	    std::cerr << entity_type << " #" << entity_id
		<< ": implicit periodic face-band candidate F" << fi
		<< "/shell" << face.m_face_user.i << " rejected: " << reason
		<< " (" << candidates.size() << " boundary loops";
	    for (std::vector<BoundaryCandidate>::const_iterator candidate =
		    candidates.begin(); candidate != candidates.end(); ++candidate) {
		std::cerr << " L" << candidate->loop_index << "/T";
		for (size_t trim_offset = 0;
			trim_offset < candidate->trim_indices.size(); ++trim_offset) {
		    if (trim_offset) std::cerr << ',';
		    std::cerr << candidate->trim_indices[trim_offset];
		}
		std::cerr << '@' << candidate->open_parameter;
	    }
	    std::cerr << ')';
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		const ON_BrepLoop &loop = brep->m_L[li];
		std::cerr << " loop L" << li << "/STEP" << loop.m_loop_user.i
		    << '[';
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    if (lti) std::cerr << ',';
		    if (!trim) {
			std::cerr << "null";
			continue;
		    }
		    const ON_3dPoint start = trim->PointAtStart();
		    const ON_3dPoint end = trim->PointAtEnd();
		    std::cerr << 'T' << trim->m_trim_index << ':' << start.x << ':'
			<< start.y << "->" << end.x << ':' << end.y;
		}
		std::cerr << ']';
	    }
	    std::cerr << std::endl;
	};
	/* More than two periodic boundary loops can encode multiple disjoint
	 * bands.  Pairing those without an additional source relationship would
	 * be a topology guess, so leave such a face unchanged. */
	if (candidates.size() != 2) {
	    report_rejection("the candidate count was not exactly two");
	    continue;
	}
	/* Exact pullbacks on a closed surface can finish a few floating-point
	 * ulps outside a native parameter boundary.  Candidate discovery already
	 * proved the complete boundary lift and winding; snap only that numerical
	 * boundary residue before trimming the analytic seam isocurve.  Without
	 * this normalization ON_Curve::Trim rejects, for example, [-3e-13, 25.9]
	 * against a [0, 52.8] torus profile even though the first endpoint is the
	 * exact native minimum in model space. */
	const ON_Interval open_domain = surface->Domain(open_direction);
	if (!open_domain.IsIncreasing()) {
	    report_rejection("the open surface parameter domain was invalid");
	    continue;
	}
	for (std::vector<BoundaryCandidate>::iterator candidate =
		candidates.begin(); candidate != candidates.end(); ++candidate) {
	    /* On a torus the parameter direction transverse to the boundary is
	     * periodic too.  A valid supplied loop may therefore meet the chosen
	     * cut on an adjacent image (for example -0.1 instead of max-0.1).
	     * Move the complete boundary by an integral period onto the native
	     * image; install_period_shifts() densely proves that exact translation
	     * before any topology is committed. */
	    if (surface->IsClosed(open_direction)) {
		const double open_period = open_domain.Length();
		if (open_period > ON_ZERO_TOLERANCE) {
		    while (candidate->open_parameter < open_domain.Min() -
			    parameter_tolerance) {
			candidate->open_parameter += open_period;
			++candidate->open_period_shift;
		    }
		    while (candidate->open_parameter > open_domain.Max() +
			    parameter_tolerance) {
			candidate->open_parameter -= open_period;
			--candidate->open_period_shift;
		    }
		}
	    }
	    if (fabs(candidate->open_parameter - open_domain.Min()) <=
		    parameter_tolerance)
		candidate->open_parameter = open_domain.Min();
	    else if (fabs(candidate->open_parameter - open_domain.Max()) <=
		    parameter_tolerance)
		candidate->open_parameter = open_domain.Max();
	}
	if (candidates[1].open_parameter < candidates[0].open_parameter)
	    std::swap(candidates[0], candidates[1]);
	BoundaryCandidate lower = candidates[0];
	BoundaryCandidate upper = candidates[1];
	if (upper.open_parameter - lower.open_parameter <= parameter_tolerance ||
		lower.vertex_index == upper.vertex_index) {
	    report_rejection("the boundary intervals or vertices were not distinct");
	    continue;
	}
	const auto boundary_tolerance = [brep](const BoundaryCandidate &boundary) {
	    double tolerance = LocalUnits::tolerance;
	    for (std::vector<int>::const_iterator ti =
		    boundary.trim_indices.begin(); ti != boundary.trim_indices.end();
		    ++ti) {
		if (*ti < 0 || *ti >= brep->m_T.Count()) continue;
		const ON_BrepTrim &trim = brep->m_T[*ti];
		tolerance = std::max(tolerance,
		    std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
		if (trim.m_ei >= 0 && trim.m_ei < brep->m_E.Count())
		    tolerance = std::max(tolerance,
			brep->m_E[trim.m_ei].m_tolerance);
	    }
	    return tolerance;
	};
	const double band_tolerance = std::max(boundary_tolerance(lower),
	    boundary_tolerance(upper));

	const ON_3dPoint &lower_vertex = brep->m_V[lower.vertex_index].point;
	const ON_3dPoint &upper_vertex = brep->m_V[upper.vertex_index].point;
	const double closed_min = closed_domain.Min();
	const double closed_max = closed_domain.Max();
	ON_3dPoint lower_uv_min;
	ON_3dPoint lower_uv_max;
	ON_3dPoint upper_uv_min;
	ON_3dPoint upper_uv_max;
	lower_uv_min[closed_direction] = closed_min;
	lower_uv_min[open_direction] = lower.open_parameter;
	lower_uv_min.z = 0.0;
	lower_uv_max = lower_uv_min;
	lower_uv_max[closed_direction] = closed_max;
	upper_uv_min[closed_direction] = closed_min;
	upper_uv_min[open_direction] = upper.open_parameter;
	upper_uv_min.z = 0.0;
	upper_uv_max = upper_uv_min;
	upper_uv_max[closed_direction] = closed_max;
	const ON_3dPoint lower_lifts[2] = {
	    surface->PointAt(lower_uv_min.x, lower_uv_min.y),
	    surface->PointAt(lower_uv_max.x, lower_uv_max.y)
	};
	const ON_3dPoint upper_lifts[2] = {
	    surface->PointAt(upper_uv_min.x, upper_uv_min.y),
	    surface->PointAt(upper_uv_max.x, upper_uv_max.y)
	};
	bool lower_endpoint_proof = true;
	bool upper_endpoint_proof = true;
	for (int side = 0; side < 2; ++side) {
	    lower_endpoint_proof = lower_endpoint_proof &&
		lower_lifts[side].IsValid() &&
		lower_lifts[side].DistanceTo(lower_vertex) <=
		    band_tolerance;
	    upper_endpoint_proof = upper_endpoint_proof &&
		upper_lifts[side].IsValid() &&
		upper_lifts[side].DistanceTo(upper_vertex) <=
		    band_tolerance;
	}
	const bool endpoint_proof =
	    lower_endpoint_proof && upper_endpoint_proof;
	if (!endpoint_proof) {
	    /* A complete periodic boundary may be cut at an existing topology
	     * vertex opposite the private OpenNURBS surface seam.  Its supplied
	     * pcurve is exact, but testing the same open parameter at the native
	     * domain sides necessarily lifts to the antipode.  Move the private
	     * seam to that proven boundary vertex and transactionally regenerate
	     * every pcurve on the shared surface.  The relocation helper rejects a
	     * cut through an ordinary edge and densely validates all regenerated
	     * lifts, so this changes only parameterization, never geometry or STEP
	     * topology. */
	    const BoundaryCandidate &non_native_boundary =
		!lower_endpoint_proof ? lower : upper;
	    const ON_BrepTrim *boundary_start =
		non_native_boundary.trim_indices.empty() ? NULL :
		brep->m_T.At(non_native_boundary.trim_indices.front());
	    const ON_3dPoint boundary_start_uv = boundary_start ?
		boundary_start->PointAtStart() : ON_3dPoint::UnsetPoint;
	    const ON_3dPoint boundary_start_lift =
		boundary_start_uv.IsValid() ?
		surface->PointAt(boundary_start_uv.x, boundary_start_uv.y) :
		ON_3dPoint::UnsetPoint;
	    if (face.m_si >= 0 && boundary_start &&
		    boundary_start->m_vi[0] == non_native_boundary.vertex_index &&
		    boundary_start_lift.IsValid() &&
		    boundary_start_lift.DistanceTo(
			brep->m_V[non_native_boundary.vertex_index].point) <=
			band_tolerance &&
		    aligned_band_surface_seams.find(face.m_si) ==
			aligned_band_surface_seams.end()) {
		std::string relocation_failure;
		if (relocate_closed_surface_loop_seam(brep,
			non_native_boundary.loop_index, closed_direction,
			band_tolerance, wrapper, entity_id, entity_type,
			&relocation_failure,
			boundary_start_uv[closed_direction])) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"aligned a closed surface seam with a proven periodic boundary topology vertex");
		    --fi;
		    continue;
		}
		aligned_band_surface_seams.insert(face.m_si);
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic face-band topology-seam alignment F" << fi
			<< "/L" << non_native_boundary.loop_index
			<< " rejected: " << relocation_failure << std::endl;
	    }
	    if (wrapper->Verbose()) {
		std::cerr << entity_type << " #" << entity_id
		    << ": periodic face-band endpoint proof F" << fi
		    << " closed-direction=" << closed_direction
		    << " closed-domain=" << closed_domain.Min() << ':'
		    << closed_domain.Max()
		    << " open-direction=" << open_direction
		    << " open-domain=" << open_domain.Min() << ':'
		    << open_domain.Max()
		    << " lower=L" << lower.loop_index << "/V"
		    << lower.vertex_index << '@' << lower.open_parameter
		    << " distances="
		    << lower_lifts[0].DistanceTo(lower_vertex) << '/'
		    << lower_lifts[1].DistanceTo(lower_vertex)
		    << " upper=L" << upper.loop_index << "/V"
		    << upper.vertex_index << '@' << upper.open_parameter
		    << " distances="
		    << upper_lifts[0].DistanceTo(upper_vertex) << '/'
		    << upper_lifts[1].DistanceTo(upper_vertex)
		    << " tolerance=" << band_tolerance << std::endl;
	    }
	    report_rejection("a periodic boundary did not lift to its STEP vertex");
	    continue;
	}

	std::unique_ptr<ON_Curve> seam_curve(surface->IsoCurve(open_direction,
	    closed_min));
	const ON_Interval seam_domain(lower.open_parameter,
	    upper.open_parameter);
	if (!seam_curve || !seam_domain.IsIncreasing() ||
		!seam_curve->Trim(seam_domain) || !seam_curve->IsValid()) {
	    report_rejection("the exact surface isocurve could not be trimmed");
	    continue;
	}
	if (seam_curve->PointAtStart().DistanceTo(lower_vertex) >
		seam_curve->PointAtEnd().DistanceTo(lower_vertex) &&
		!seam_curve->Reverse())
	    continue;
	bool seam_proof = true;
	const ON_Interval curve_domain = seam_curve->Domain();
	for (int sample = 0; seam_proof && sample <= kDenseValidationSegments;
		++sample) {
	    if (brlcad::PullbackWorkCancelled()) {
		seam_proof = false;
		break;
	    }
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double open_parameter = seam_domain.ParameterAt(fraction);
	    ON_3dPoint uv_min;
	    ON_3dPoint uv_max;
	    uv_min[closed_direction] = closed_min;
	    uv_min[open_direction] = open_parameter;
	    uv_min.z = 0.0;
	    uv_max = uv_min;
	    uv_max[closed_direction] = closed_max;
	    const ON_3dPoint curve_point = seam_curve->PointAt(
		curve_domain.ParameterAt(fraction));
	    const ON_3dPoint min_lift = surface->PointAt(uv_min.x, uv_min.y);
	    const ON_3dPoint max_lift = surface->PointAt(uv_max.x, uv_max.y);
	    seam_proof = curve_point.IsValid() && min_lift.IsValid() &&
		max_lift.IsValid() &&
		curve_point.DistanceTo(min_lift) <= band_tolerance &&
		curve_point.DistanceTo(max_lift) <= band_tolerance;
	}
	if (!seam_proof || seam_curve->PointAtStart().DistanceTo(lower_vertex) >
		band_tolerance ||
		seam_curve->PointAtEnd().DistanceTo(upper_vertex) >
		band_tolerance) {
	    report_rejection("the surface seam did not pass dense lift validation");
	    continue;
	}

	std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	const auto install_period_shifts = [brep, surface, closed_direction,
		open_direction, &closed_domain, &open_domain](
		const BoundaryCandidate &boundary) {
	    if (boundary.trim_indices.size() != boundary.period_shifts.size())
		return false;
	    for (size_t offset = 0; offset < boundary.trim_indices.size(); ++offset) {
		const int ti = boundary.trim_indices[offset];
		const int period_shift = boundary.period_shifts[offset];
		if (period_shift == 0 && boundary.open_period_shift == 0)
		    continue;
		if (ti < 0 || ti >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &trim = brep->m_T[ti];
		std::unique_ptr<ON_Curve> translated(trim.DuplicateCurve());
		ON_Xform transform(ON_Xform::IdentityTransformation);
		transform.m_xform[closed_direction][3] =
		    period_shift * closed_domain.Length();
		transform.m_xform[open_direction][3] =
		    boundary.open_period_shift * open_domain.Length();
		std::string failure;
		if (!translated || !translated->Transform(transform) ||
			!translated->ChangeDimension(2) || !translated->IsValid() ||
			!validate_periodic_trim_translation(surface, trim,
			    *translated, &failure))
		    return false;
		const int c2 = brep->AddTrimCurve(translated.release());
		if (c2 < 0 || !brep->SetTrimCurve(trim, c2))
		    return false;
		brep->SetTrimIsoFlags(trim);
	    }
	    return true;
	};
	if (!install_period_shifts(lower) || !install_period_shifts(upper)) {
	    report_rejection("an integral-period boundary translation failed dense lift validation");
	    *brep = *rollback;
	    continue;
	}
	size_t snapped_band_joins = 0;
	const auto snap_boundary_joins = [brep, surface, &snapped_band_joins](
		const BoundaryCandidate &boundary) {
	    for (size_t offset = 1; offset < boundary.trim_indices.size(); ++offset) {
		const int previous_index = boundary.trim_indices[offset - 1];
		const int next_index = boundary.trim_indices[offset];
		if (previous_index < 0 || previous_index >= brep->m_T.Count() ||
			next_index < 0 || next_index >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &previous = brep->m_T[previous_index];
		ON_BrepTrim &next = brep->m_T[next_index];
		const ON_3dPoint previous_uv = previous.PointAtEnd();
		const ON_3dPoint next_uv = next.PointAtStart();
		if (previous_uv.DistanceTo(next_uv) <= ON_ZERO_TOLERANCE)
		    continue;
		if (previous.m_vi[1] < 0 || previous.m_vi[1] != next.m_vi[0] ||
			previous.m_vi[1] >= brep->m_V.Count())
		    return false;
		double tolerance = LocalUnits::tolerance;
		if (previous.Edge())
		    tolerance = std::max(tolerance, previous.Edge()->m_tolerance);
		if (next.Edge())
		    tolerance = std::max(tolerance, next.Edge()->m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(previous.m_tolerance[0], previous.m_tolerance[1]));
		tolerance = std::max(tolerance,
		    std::max(next.m_tolerance[0], next.m_tolerance[1]));
		tolerance = std::max(tolerance,
		    brep->m_V[previous.m_vi[1]].m_tolerance);
		const ON_3dPoint previous_lift =
		    closed_surface_point_at(surface, previous_uv);
		const ON_3dPoint next_lift =
		    closed_surface_point_at(surface, next_uv);
		const ON_3dPoint common = 0.5 * (previous_uv + next_uv);
		const ON_3dPoint common_lift = surface->PointAt(common.x, common.y);
		const ON_3dPoint &vertex = brep->m_V[previous.m_vi[1]].point;
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			!common_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > tolerance ||
			next_lift.DistanceTo(vertex) > tolerance ||
			common_lift.DistanceTo(vertex) > tolerance)
		    return false;

		std::unique_ptr<ON_Curve> previous_candidate(
		    previous.DuplicateCurve());
		std::unique_ptr<ON_Curve> next_candidate(next.DuplicateCurve());
		if (!previous_candidate || !next_candidate ||
			!previous_candidate->SetEndPoint(common) ||
			!next_candidate->SetStartPoint(common) ||
			!previous_candidate->ChangeDimension(2) ||
			!next_candidate->ChangeDimension(2) ||
			!previous_candidate->IsValid() || !next_candidate->IsValid())
		    return false;
		const auto preserves_lift = [surface, tolerance](
			const ON_BrepTrim &original, const ON_Curve &candidate) {
		    const ON_Interval domain = original.Domain();
		    const int samples = std::min(4096, std::max(64,
			std::max(original.SpanCount(), candidate.SpanCount()) * 8));
		    for (int sample = 0; sample <= samples; ++sample) {
			if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
			    return false;
			const double parameter = domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = original.PointAt(parameter);
			const ON_3dPoint candidate_uv = candidate.PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint candidate_lift = closed_surface_point_at(
			    surface, candidate_uv);
			if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
				original_lift.DistanceTo(candidate_lift) > tolerance)
			    return false;
		    }
		    return true;
		};
		if (!preserves_lift(previous, *previous_candidate) ||
			!preserves_lift(next, *next_candidate))
		    return false;
		const int previous_c2 = brep->AddTrimCurve(
		    previous_candidate.release());
		const int next_c2 = brep->AddTrimCurve(next_candidate.release());
		if (previous_c2 < 0 || next_c2 < 0 ||
			!brep->SetTrimCurve(previous, previous_c2) ||
			!brep->SetTrimCurve(next, next_c2))
		    return false;
		brep->SetTrimIsoFlags(previous);
		brep->SetTrimIsoFlags(next);
		++snapped_band_joins;
	    }
	    return true;
	};
	if (!snap_boundary_joins(lower) || !snap_boundary_joins(upper)) {
	    report_rejection("a proven boundary join could not be closed within its measured topology tolerance");
	    *brep = *rollback;
	    continue;
	}
	const auto orient_boundary = [brep, closed_direction, closed_min,
		closed_max, parameter_tolerance](std::vector<int> &trims,
		bool increasing) {
	    if (trims.empty()) return false;
	    ON_3dPoint start = brep->m_T[trims.front()].PointAtStart();
	    ON_3dPoint end = brep->m_T[trims.back()].PointAtEnd();
	    const bool already_oriented = increasing ?
		(fabs(start[closed_direction] - closed_min) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_max) <= parameter_tolerance) :
		(fabs(start[closed_direction] - closed_max) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_min) <= parameter_tolerance);
	    if (already_oriented)
		return true;
	    const bool reversed_orientation = increasing ?
		(fabs(start[closed_direction] - closed_max) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_min) <= parameter_tolerance) :
		(fabs(start[closed_direction] - closed_min) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_max) <= parameter_tolerance);
	    if (!reversed_orientation)
		return false;
	    std::reverse(trims.begin(), trims.end());
	    for (std::vector<int>::iterator trim = trims.begin();
		    trim != trims.end(); ++trim)
		if (*trim < 0 || *trim >= brep->m_T.Count() ||
			!brep->m_T[*trim].Reverse())
		    return false;
	    return true;
	};
	if (!orient_boundary(lower.trim_indices, true) ||
		!orient_boundary(upper.trim_indices, false)) {
	    report_rejection("the supplied boundary directions were inconsistent");
	    *brep = *rollback;
	    continue;
	}

	const ON_3dPoint lower_start =
	    brep->m_T[lower.trim_indices.front()].PointAtStart();
	const ON_3dPoint lower_end =
	    brep->m_T[lower.trim_indices.back()].PointAtEnd();
	const ON_3dPoint upper_start =
	    brep->m_T[upper.trim_indices.front()].PointAtStart();
	const ON_3dPoint upper_end =
	    brep->m_T[upper.trim_indices.back()].PointAtEnd();
	if (lower_start.DistanceTo(lower_uv_min) > parameter_tolerance ||
		lower_end.DistanceTo(lower_uv_max) > parameter_tolerance ||
		upper_start.DistanceTo(upper_uv_max) > parameter_tolerance ||
		upper_end.DistanceTo(upper_uv_min) > parameter_tolerance) {
	    report_rejection("the supplied boundaries were not on the native periodic sides");
	    *brep = *rollback;
	    continue;
	}
	/* Use the existing exact boundary endpoints so all four UV joins are
	 * identical to parameter-space numerical precision. */
	std::unique_ptr<ON_LineCurve> maximum_trim_curve(new ON_LineCurve(
	    lower_end, upper_start));
	std::unique_ptr<ON_LineCurve> minimum_trim_curve(new ON_LineCurve(
	    upper_end, lower_start));
	if (!maximum_trim_curve->ChangeDimension(2) ||
		!minimum_trim_curve->ChangeDimension(2) ||
		!maximum_trim_curve->IsValid() ||
		!minimum_trim_curve->IsValid()) {
	    *brep = *rollback;
	    continue;
	}

	const int seam_c3 = brep->AddEdgeCurve(seam_curve.release());
	const int maximum_c2 = brep->AddTrimCurve(maximum_trim_curve.release());
	const int minimum_c2 = brep->AddTrimCurve(minimum_trim_curve.release());
	if (seam_c3 < 0 || maximum_c2 < 0 || minimum_c2 < 0) {
	    *brep = *rollback;
	    continue;
	}
	ON_BrepEdge &seam_edge = brep->NewEdge(
	    brep->m_V[lower.vertex_index], brep->m_V[upper.vertex_index],
	    seam_c3, NULL, band_tolerance);
	seam_edge.m_tolerance = band_tolerance;
	const int seam_edge_index = seam_edge.m_edge_index;
	const int maximum_trim_index = brep->NewTrim(seam_edge, false,
	    brep->m_L[lower.loop_index], maximum_c2).m_trim_index;
	const int minimum_trim_index = brep->NewTrim(
	    brep->m_E[seam_edge_index], true, brep->m_L[lower.loop_index],
	    minimum_c2).m_trim_index;
	if (maximum_trim_index < 0 || minimum_trim_index < 0) {
	    *brep = *rollback;
	    continue;
	}

	ON_BrepLoop &combined_loop = brep->m_L[lower.loop_index];
	ON_BrepLoop &discarded_loop = brep->m_L[upper.loop_index];
	const bool merged_supplied_outer_bounds =
	    combined_loop.m_type == ON_BrepLoop::outer &&
	    discarded_loop.m_type == ON_BrepLoop::outer;
	combined_loop.m_ti.SetCount(0);
	std::vector<int> ordered_trims(lower.trim_indices.begin(),
	    lower.trim_indices.end());
	ordered_trims.push_back(maximum_trim_index);
	ordered_trims.insert(ordered_trims.end(), upper.trim_indices.begin(),
	    upper.trim_indices.end());
	ordered_trims.push_back(minimum_trim_index);
	for (std::vector<int>::const_iterator ordered = ordered_trims.begin();
		ordered != ordered_trims.end(); ++ordered) {
	    combined_loop.m_ti.Append(*ordered);
	    brep->m_T[*ordered].m_li = lower.loop_index;
	    brep->m_T[*ordered].m_tolerance[0] =
		band_tolerance;
	    brep->m_T[*ordered].m_tolerance[1] =
		band_tolerance;
	}
	combined_loop.m_type = ON_BrepLoop::outer;
	brep->m_T[maximum_trim_index].m_type = ON_BrepTrim::seam;
	brep->m_T[minimum_trim_index].m_type = ON_BrepTrim::seam;
	brep->m_T[maximum_trim_index].m_iso = closed_direction == 0 ?
	    ON_Surface::E_iso : ON_Surface::N_iso;
	brep->m_T[minimum_trim_index].m_iso = closed_direction == 0 ?
	    ON_Surface::W_iso : ON_Surface::S_iso;
	discarded_loop.m_ti.SetCount(0);
	brep->DeleteLoop(discarded_loop, false);
	if (!brep->Compact()) {
	    *brep = *rollback;
	    continue;
	}
	std::string unsafe_topology;
	if (!brep_topology_references_are_safe(brep, &unsafe_topology)) {
	    *brep = *rollback;
	    continue;
	}
	/* Candidate discovery proves periodic winding and model-space lift, but a
	 * boundary reversal also changes each trim's directed 3-D edge use.  Make
	 * the face modification transactional with respect to OpenNURBS' own trim,
	 * loop, and face invariants; otherwise an ambiguous torus band can survive
	 * this pass and fail much later with a pcurve endpoint attached to the wrong
	 * end of an otherwise exact edge. */
	ON_wString band_messages;
	ON_TextLog band_log(band_messages);
	bool valid_band = fi >= 0 && fi < brep->m_F.Count();
	const ON_BrepFace *installed_face = valid_band ? &brep->m_F[fi] : NULL;
	for (int fli = 0; valid_band && fli < installed_face->m_li.Count(); ++fli) {
	    const int installed_loop_index = installed_face->m_li[fli];
	    if (installed_loop_index < 0 ||
		    installed_loop_index >= brep->m_L.Count()) {
		valid_band = false;
		break;
	    }
	    const ON_BrepLoop &installed_loop =
		brep->m_L[installed_loop_index];
	    for (int lti = 0; valid_band && lti < installed_loop.TrimCount();
		    ++lti) {
		const ON_BrepTrim *installed_trim = installed_loop.Trim(lti);
		valid_band = installed_trim && installed_trim->IsValid(&band_log);
	    }
	    valid_band = valid_band && installed_loop.IsValid(&band_log);
	}
	valid_band = valid_band && installed_face->IsValid(&band_log);
	if (!valid_band) {
	    if (wrapper->Verbose()) {
		ON_String text(band_messages);
		std::cerr << entity_type << " #" << entity_id
		    << ": periodic face-band candidate F" << fi
		    << " failed transactional OpenNURBS validation:\n"
		    << text.Array();
	    }
	    *brep = *rollback;
	    continue;
	}
	++repaired;
	for (size_t snapped_join = 0; snapped_join < snapped_band_joins;
		snapped_join++)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"closed a proven periodic-band boundary join within its measured topology tolerance");
	for (int translated_boundary = 0;
		translated_boundary < (lower.open_period_shift != 0 ? 1 : 0) +
		    (upper.open_period_shift != 0 ? 1 : 0);
		++translated_boundary)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"translated a complete band boundary onto an exact adjacent periodic surface image");
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band");
	if (merged_supplied_outer_bounds)
	    wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		"merged multiple supplied FACE_OUTER_BOUND components into one exact periodic face boundary");
	}
    const long long elapsed_microseconds =
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - pass_started).count();
    wrapper->RecordStageTiming("periodic_band_repair", entity_id,
	entity_type, static_cast<uint64_t>(std::max<long long>(0,
	    elapsed_microseconds)), static_cast<uint64_t>(brep->m_F.Count()),
	static_cast<uint64_t>(brep->m_E.Count()),
	static_cast<uint64_t>(brep->m_T.Count()));
    if (wrapper->Verbose()) {
	std::cerr << entity_type << " #" << entity_id
	    << ": periodic-band pass telemetry elapsed="
	    << elapsed_microseconds / 1000.0 << "ms faces="
	    << periodic_faces_visited << " closed-singletons="
	    << closed_singletons_seen << " periodic-singletons="
	    << periodic_singletons_seen << " supplied-winding="
	    << supplied_winding_candidates << " arbitrary-splits="
	    << arbitrary_split_successes << '/' << arbitrary_split_attempts
	    << " arbitrary-time=" << arbitrary_split_microseconds / 1000.0
	    << "ms segmented-scans=" << segmented_chain_scans
	    << " collapsed=" << collapsed_boundary_successes << '/'
	    << collapsed_boundary_attempts << " collapsed-time="
	    << collapsed_boundary_microseconds / 1000.0 << "ms full-period="
	    << full_period_chain_successes << '/' << full_period_chain_attempts
	    << " full-period-time=" << full_period_chain_microseconds / 1000.0
	    << "ms"
	    << " cancelled=" << (brlcad::PullbackWorkCancelled() ? "yes" : "no")
	    << std::endl;
    }
    return repaired;
}


static size_t
finalize_brep_topology(ON_Brep *brep, bool normalize_keyholes,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool *topology_changed = NULL)
{
    if (topology_changed) *topology_changed = false;
    if (!brep)
	return 0;

    /* FACE_OUTER_BOUND and FACE_BOUND are authoritative.  Recomputing loop
     * types from still-unrepaired pcurves can turn a declared outer loop into
     * an inner loop and make an otherwise exact face structurally invalid.
     * Trim types still need the complete edge-use graph. */
    if (wrapper)
	wrapper->SetProgressDetail("refreshing exact BREP topology flags",
	    entity_id, 0, 0, std::string(), entity_type);
    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
	entity_id, &entity_type);
    if (brlcad::PullbackWorkCancelled())
	return 0;
    if (wrapper)
	wrapper->SetProgressDetail("classifying exact BREP seams", entity_id,
	    0, 0, std::string(), entity_type);
    classify_exact_polyline_seams(brep);
    if (brlcad::PullbackWorkCancelled())
	return 0;
    size_t keyhole_splits = 0;
    size_t keyhole_rejections = 0;
    const size_t keyhole_diagnostic_limit = 16;
    if (normalize_keyholes) {
	if (wrapper)
	    wrapper->SetProgressDetail("normalizing exact BREP keyhole loops",
		entity_id, 0, static_cast<uint64_t>(brep->m_L.Count()), "loops",
		entity_type);
	bool removed_slit = true;
	while (removed_slit) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    removed_slit = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		if (brlcad::PullbackWorkCancelled())
		    return keyhole_splits;
		if (!remove_adjacent_zero_area_slit(brep, li))
		    continue;
		if (wrapper)
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"removed an exact zero-area adjacent seam bridge");
		if (topology_changed) *topology_changed = true;
		refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		    entity_id, &entity_type);
		classify_exact_polyline_seams(brep);
		removed_slit = true;
		break;
	    }
	}
	bool changed = true;
	while (changed) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    changed = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		if (brlcad::PullbackWorkCancelled())
		    return keyhole_splits;
		std::string split_failure;
		if (split_keyhole_loop(brep, li, &split_failure)) {
		    ++keyhole_splits;
		    if (topology_changed) *topology_changed = true;
		    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
			entity_id, &entity_type);
		    classify_exact_polyline_seams(brep);
		    changed = true;
		    break;
		}
		if (wrapper && wrapper->Verbose() && !split_failure.empty()) {
		    ++keyhole_rejections;
		    if (keyhole_rejections <= keyhole_diagnostic_limit) {
			std::cerr << entity_type << " #" << entity_id << ": keyhole loop "
			    << li << " split rejected: " << split_failure << std::endl;
			wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
			    entity_id, entity_type, "edge_loop",
			    "keyhole split rejected: " + split_failure);
		    }
		}
	    }
	}
    }
    if (wrapper && wrapper->Verbose() && keyhole_rejections > keyhole_diagnostic_limit)
	std::cerr << entity_type << " #" << entity_id << ": suppressed "
	    << keyhole_rejections - keyhole_diagnostic_limit
	    << " additional exact keyhole split rejections ("
	    << keyhole_rejections << " total)" << std::endl;
    if (wrapper)
	wrapper->SetProgressDetail("sorting exact BREP face loops", entity_id,
	    0, static_cast<uint64_t>(brep->m_F.Count()), "faces", entity_type);
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (brlcad::PullbackWorkCancelled())
	    return keyhole_splits;
	brep->SortFaceLoops(brep->m_F[fi]);
    }
    return keyhole_splits;
}


static void
closed_trim_endpoint_alignments(const ON_BrepTrim &trim, const ON_BrepEdge &edge,
	const ON_Surface *surface, double alignment[2])
{
    alignment[0] = ON_UNSET_VALUE;
    alignment[1] = ON_UNSET_VALUE;
    if (!surface)
	return;
    const ON_Interval trim_domain = trim.Domain();
    for (int end = 0; end < 2; ++end) {
	ON_3dPoint uv, lifted_point;
	ON_3dVector uv_tangent, du, dv;
	const double trim_parameter = trim_domain[end];
	const double edge_parameter = edge.Domain()[trim.m_bRev3d ? 1 - end : end];
	if (!trim.Ev1Der(trim_parameter, uv, uv_tangent) ||
		!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
	    continue;
	ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	ON_3dVector edge_tangent = edge.TangentAt(edge_parameter);
	if (!lifted_tangent.Unitize() || !edge_tangent.Unitize())
	    continue;
	alignment[end] = lifted_tangent * edge_tangent;
	if (trim.m_bRev3d)
	    alignment[end] = -alignment[end];
    }
}


static bool
closed_trim_endpoint_alignment_is_valid(double alignment)
{
    /* ON_UNSET_VALUE is deliberately a large negative sentinel.  Comparing
     * it directly with zero mistakes an unevaluable tangent (most commonly
     * at a collapsed surface pole) for strong evidence of reversed geometry
     * and can launch an unnecessary dense pcurve regeneration. */
    return ON_IsValid(alignment) && alignment > ON_UNSET_VALUE;
}


static bool
refine_surface_pullback_seeded(const ON_Surface *surface, const ON_3dPoint &target,
    double tolerance, ON_3dPoint &uv, double *final_distance,
    bool confine_to_domain = false, double acceptance_tolerance = 0.0)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;
    if (!(acceptance_tolerance > 0.0))
	acceptance_tolerance = tolerance;

    if (confine_to_domain) {
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    uv[direction] = std::max(domain.Min(),
		std::min(domain.Max(), uv[direction]));
	}
    }

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint lifted;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, lifted, du, dv) || !lifted.IsValid())
	    break;
	const ON_3dVector residual = target - lifted;
	best_distance = residual.Length();
	if (best_distance <= tolerance) {
	    if (final_distance)
		*final_distance = best_distance;
	    return true;
	}

	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = metric_scale * 1.0e-14;
	const double aa = a + damping;
	const double cc = c + damping;
	const double determinant = aa * cc - b * b;
	if (fabs(determinant) <= DBL_EPSILON * metric_scale * metric_scale)
	    break;
	double delta[2] = {(cc * r0 - b * r1) / determinant,
	    (aa * r1 - b * r0) / determinant};
	double trust_scale = 1.0;
	for (int direction = 0; direction < 2; ++direction) {
	    const double maximum_step = 0.125 * surface->Domain(direction).Length();
	    if (maximum_step > ON_ZERO_TOLERANCE && fabs(delta[direction]) > maximum_step)
		trust_scale = std::min(trust_scale, maximum_step / fabs(delta[direction]));
	}

	bool improved = false;
	for (int reduction = 0; reduction < 12; ++reduction) {
	    const double scale = trust_scale * std::ldexp(1.0, -reduction);
	    ON_3dPoint candidate(uv.x + scale * delta[0],
		uv.y + scale * delta[1], 0.0);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!confine_to_domain && surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		candidate[direction] = std::max(domain.Min(),
		    std::min(domain.Max(), candidate[direction]));
	    }
	    const ON_3dPoint candidate_lift = surface->PointAt(candidate.x, candidate.y);
	    if (!candidate_lift.IsValid())
		continue;
	    const double candidate_distance = candidate_lift.DistanceTo(target);
	    if (candidate_distance < best_distance) {
		uv = candidate;
		best_distance = candidate_distance;
		improved = true;
		break;
	    }
	}
	if (!improved)
	    break;
    }
    if (final_distance)
	*final_distance = best_distance;
    return best_distance <= acceptance_tolerance;
}


static void
normalize_closed_surface_parameter(const ON_Surface *surface,
	const ON_3dPoint &target, double tolerance, ON_3dPoint &uv)
{
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		(uv[direction] >= domain.Min() && uv[direction] <= domain.Max()))
	    continue;
	double wrapped = fmod(uv[direction] - domain.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	ON_3dPoint candidate = uv;
	candidate[direction] = domain.Min() + wrapped;
	const ON_3dPoint lifted = surface->PointAt(candidate.x, candidate.y);
	if (lifted.IsValid() && lifted.DistanceTo(target) <= tolerance)
	    uv = candidate;
    }
}


static bool
has_unwrapped_closed_parameter(const ON_Surface *surface, const ON_3dPoint &uv)
{
    if (!surface || !uv.IsValid())
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double guard = std::max(ON_ZERO_TOLERANCE,
	    domain.Length() * 1.0e-12);
	if (uv[direction] < domain.Min() - guard ||
		uv[direction] > domain.Max() + guard)
	    return true;
    }
    return false;
}


/* Evaluate a mathematically closed surface at a continuous, possibly
 * unwrapped UV image without asking a non-periodic NURBS implementation to
 * extrapolate beyond its native knot domain.  The returned native parameter
 * is lift-equivalent by the surface's own IsClosed() contract.  Intermediate
 * pcurves may use at most one such image while periodic topology is being
 * split; final OpenNURBS validation still requires native-domain trims. */
static ON_3dPoint
closed_surface_native_parameter(const ON_Surface *surface, ON_3dPoint uv)
{
    if (!surface || !uv.IsValid())
	return ON_3dPoint::UnsetPoint;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		(uv[direction] >= domain.Min() &&
		 uv[direction] <= domain.Max()))
	    continue;
	double wrapped = fmod(uv[direction] - domain.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	uv[direction] = domain.Min() + wrapped;
    }
    return uv;
}


static ON_3dPoint
closed_surface_point_at(const ON_Surface *surface, const ON_3dPoint &uv)
{
    const ON_3dPoint native = closed_surface_native_parameter(surface, uv);
    return surface && native.IsValid() ?
	surface->PointAt(native.x, native.y) : ON_3dPoint::UnsetPoint;
}


static bool
closed_surface_ev1der(const ON_Surface *surface, const ON_3dPoint &uv,
	ON_3dPoint &point, ON_3dVector &du, ON_3dVector &dv)
{
    const ON_3dPoint native = closed_surface_native_parameter(surface, uv);
    return surface && native.IsValid() &&
	surface->Ev1Der(native.x, native.y, point, du, dv);
}


struct PeriodicPullbackCrossing {
    bool detected = false;
    double trim_fraction = 0.0;
    int surface_direction = -1;
};


static bool
regenerate_trim_polyline(ON_Brep *brep, ON_BrepTrim &trim,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs, double tolerance,
	std::string *failure_reason, PeriodicPullbackCrossing *periodic_crossing = NULL,
	const ON_3dPoint *required_start = NULL,
	const ON_3dPoint *required_end = NULL,
	bool prefer_edge_driven = false, STEPWrapper *wrapper = NULL,
	bool preserve_required_uv_images = false,
	ON_Curve **generated_curve = NULL)
{
    if (failure_reason)
	failure_reason->clear();
    if (periodic_crossing)
	*periodic_crossing = PeriodicPullbackCrossing();
    if (generated_curve)
	*generated_curve = NULL;
    if (!brep || !surface || !(tolerance > 0.0))
	return false;

    const ON_BrepEdge *edge = trim.Edge();
    if (!edge)
	return false;
    const ON_Interval trim_domain = trim.Domain();
    brlcad::PullbackContext pullback_context;
    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_feature_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    const auto pullback_solver_tolerance = [edge_feature_scale](
	    double acceptance_tolerance) {
	double solver_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    acceptance_tolerance * 0.1);
	if (edge_feature_scale > ON_ZERO_TOLERANCE &&
		acceptance_tolerance > edge_feature_scale *
		    kPullbackLooseToleranceFeatureFraction)
	    solver_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		std::min(solver_tolerance, edge_feature_scale *
		    kPullbackSolverFeatureFraction));
	return solver_tolerance;
    };
    std::string edge_driven_failure;
    for (int regeneration_mode = 0; regeneration_mode < 2; ++regeneration_mode) {
    const bool edge_driven = prefer_edge_driven ? regeneration_mode == 0 :
	regeneration_mode == 1;
    const int maximum_segments = edge_driven ? 64 : kDenseValidationSegments;
    for (int segment_count = 64; segment_count <= maximum_segments; segment_count *= 2) {
	std::string rejection = "unknown rejection";
	ON_3dPointArray points;
	points.Reserve(segment_count + 1);
	std::vector<double> normalized_parameters;
	normalized_parameters.reserve(segment_count + 3);
	bool valid = true;
	for (int sample = 0; sample <= segment_count; ++sample) {
	    if (wrapper && (sample % 8) == 0) {
		std::ostringstream detail;
		detail << "trim=T" << trim.m_trim_index << " sample="
		    << sample << '/' << segment_count << " mode="
		    << (edge_driven ? "edge" : "source");
		wrapper->SetProgressDetail("sampling exact edge pullback", 0,
		    static_cast<uint64_t>(sample),
		    static_cast<uint64_t>(segment_count), "samples", detail.str());
	    }
	    if (brlcad::PullbackWorkCancelled()) {
		if (failure_reason)
		    *failure_reason = "regenerated-pcurve sampling was cancelled";
		return false;
	    }
	    const double normalized = static_cast<double>(sample) /
		static_cast<double>(segment_count);
	    const ON_3dPoint *required_endpoint = sample == 0 ? required_start :
		(sample == segment_count ? required_end : NULL);
	    const ON_3dPoint source_uv = required_endpoint ? *required_endpoint :
		trim.PointAt(trim_domain.ParameterAt(normalized));
	    ON_3dPoint uv = source_uv;
	    if (edge_driven && sample > 0 && sample < segment_count) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		double pullback_distance = DBL_MAX;
		/* An exact surface point can have several parameter-space preimages at
		 * a pole or collapsed boundary.  Continue from the preceding exact
		 * sample before consulting the independently parameterized source
		 * pcurve, so adjacent samples remain on one lift-equivalent branch. */
		const bool have_continuation_seed = points.Count() > 0;
		if (have_continuation_seed)
		    uv = points[points.Count() - 1];
		bool pulled = refine_surface_pullback_seeded(surface, edge_point,
		    solver_tolerance, uv, &pullback_distance,
		    !has_unwrapped_closed_parameter(surface, uv),
		    pullback_tolerance);
		if (!pulled && have_continuation_seed) {
		    uv = source_uv;
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, uv),
			pullback_tolerance);
		}
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
			    pulled_lift, pullback_distance, 0,
			    solver_tolerance,
			    pullback_tolerance) || pullback_distance > pullback_tolerance) {
			rejection = "exact edge pullback failed";
			valid = false;
			break;
		    }
		    uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    /* Select the whole-domain branch nearest the supplied pcurve, but
		     * retain a shift only when its lift proves it is the same exact edge
		     * point.  This also handles imported periodic geometry whose closure
		     * flag was not retained by the NURBS representation. */
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round(
			    (source_uv[direction] - uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
		/* A seeded Newton solve may converge on an extrapolated image of a
		 * geometrically closed but non-periodic NURBS surface.  Anchor every
		 * successful projection—not
		 * only the global closest-point fallback—to the image nearest the
		 * supplied pcurve before enforcing continuity with the preceding
		 * sample.  Accept only an exact integral-period shift whose lift still
		 * matches the authoritative edge.  Without this anchor, successive
		 * samples can drift by dozens of whole periods and force pathological
		 * adaptive subdivision. */
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction) ||
			    surface->IsPeriodic(direction))
			continue;
		    const double direction_period =
			surface->Domain(direction).Length();
		    if (!(direction_period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = uv;
		    shifted_uv[direction] += round((source_uv[direction] -
			uv[direction]) / direction_period) * direction_period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			uv = shifted_uv;
		}
		bool excessive_period_drift = false;
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction) ||
			    surface->IsPeriodic(direction))
			continue;
		    const double direction_period =
			surface->Domain(direction).Length();
		    excessive_period_drift = excessive_period_drift ||
			(direction_period > ON_ZERO_TOLERANCE &&
			 fabs(uv[direction] - source_uv[direction]) >
			     kMaximumSeededPullbackPeriodDrift * direction_period);
		}
		if (excessive_period_drift) {
		    ON_2dPoint native_uv;
		    ON_3dPoint native_lift;
		    double native_distance = DBL_MAX;
		    if (pullback_context.SurfaceClosestPoint(surface, edge_point,
			    native_uv, native_lift, native_distance, 0,
			    solver_tolerance, pullback_tolerance) &&
			    native_distance <= pullback_tolerance)
			uv.Set(native_uv.x, native_uv.y, 0.0);
		}
		/* Keep consecutive exact samples on one lift-equivalent domain branch.
		 * The supplied curve may legitimately cross a parameter seam; using its
		 * independently wrapped values would create a full-domain chord. */
		if (points.Count() > 0) {
		    const ON_3dPoint previous_uv = points[points.Count() - 1];
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((previous_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
	    } else if (edge_driven) {
		/* Endpoints are shared with adjacent trims in the same p-space loop.
		 * Refine them to the authoritative edge endpoint when possible; a
		 * merely in-tolerance source endpoint can otherwise make the final
		 * polyline chord backtrack.  If exact refinement is unavailable,
		 * preserve an already valid source endpoint for the bounded join pass. */
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const ON_3dPoint endpoint_lift = closed_surface_point_at(surface, uv);
		const bool source_endpoint_valid = endpoint_lift.IsValid() &&
		    endpoint_lift.DistanceTo(edge_point) <= tolerance;
		/* Keep numerical convergence tighter than the established geometric
		 * acceptance tolerance, but do not use that solver target as a second,
		 * undocumented rejection threshold.  A safely measured edge/surface
		 * mismatch can exceed one tenth of the local tolerance even though the
		 * exact closest surface point is well inside the fully validated bound.
		 * Conflating the two made closed edges retain an arbitrary cyclic pcurve
		 * start when their authoritative STEP endpoint could not satisfy the
		 * unnecessarily tighter threshold. */
		const double endpoint_acceptance_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    endpoint_acceptance_tolerance);
		ON_3dPoint refined_uv = uv;
		double pullback_distance = DBL_MAX;
		bool pulled = required_endpoint && source_endpoint_valid;
		if (pulled)
		    pullback_distance = endpoint_lift.DistanceTo(edge_point);
		else
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, refined_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, refined_uv),
			endpoint_acceptance_tolerance);
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pullback_distance, 0,
			solver_tolerance, endpoint_acceptance_tolerance) &&
			pullback_distance <= endpoint_acceptance_tolerance;
		    if (pulled)
			refined_uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		}
		if (pulled)
		    uv = refined_uv;
		else if (!source_endpoint_valid) {
		    rejection = "exact edge endpoint pullback failed";
		    valid = false;
		    break;
		}
		if (pulled) {
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((source_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() && shifted_lift.DistanceTo(
				edge_point) <= endpoint_acceptance_tolerance)
			    uv = shifted_uv;
		    }
		}
	    }
	    const bool preserve_required_image = preserve_required_uv_images &&
		((sample == 0 && required_start) ||
		 (sample == segment_count && required_end));
	    if (edge_driven && points.Count() > 0 && !preserve_required_image) {
		const ON_3dPoint previous_uv = points[points.Count() - 1];
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = uv;
		    shifted_uv[direction] += std::round((previous_uv[direction] -
			uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    /* A required loop endpoint is authoritative as a 3-D locus, not as
		     * a particular periodic image.  Prefer its lift-equivalent image
		     * adjacent to the regenerated interior; the subsequent bounded
		     * loop pass translates the neighboring complete pcurve to the same
		     * image and revalidates it. */
		    if (shifted_lift.IsValid() && shifted_lift.DistanceTo(edge_point) <=
			    tolerance)
			uv = shifted_uv;
		}
	    }
	    if (edge_driven && !required_endpoint) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		normalize_closed_surface_parameter(surface,
		    edge->PointAt(edge_parameter), tolerance, uv);
	    }
	    if (!uv.IsValid()) {
		rejection = edge_driven ? "invalid exact-edge pullback sample" :
		    "invalid source pcurve sample";
		valid = false;
		break;
	    }
	    points.Append(ON_3dPoint(uv.x, uv.y, 0.0));
	    normalized_parameters.push_back(normalized);
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " while sampling " +
		    std::to_string(segment_count) + " pcurve segments";
	    continue;
	}
	if (edge_driven && points.Count() >= 3) {
	    /* A surface singularity can give an edge endpoint several exact UV
	     * preimages.  Pulling it independently from the supplied endpoint may
	     * select a different branch than the exact interior samples, leaving an
	     * unsplittable parameter-space jump.  Re-pull from the adjacent exact
	     * sample and retain it only under a tighter edge-lift bound. */
	    for (int end = 0; end < 2; ++end) {
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		const bool required_endpoint = end == 0 ? required_start != NULL :
		    required_end != NULL;
		if (required_endpoint && preserve_required_uv_images)
		    continue;
		if (required_endpoint && !surface->IsClosed(0) &&
			!surface->IsClosed(1))
		    continue;
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		ON_3dPoint continuous_uv = points[adjacent_index];
		double pullback_distance = DBL_MAX;
		if (!refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, continuous_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, continuous_uv),
			pullback_tolerance))
		    continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = continuous_uv;
		    shifted_uv[direction] += std::round((points[adjacent_index][direction] -
			continuous_uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			continuous_uv = shifted_uv;
		}
		const ON_3dPoint continuous_lift = closed_surface_point_at(
		    surface, continuous_uv);
		/* A required endpoint fixes the exact 3-D loop join, not a particular
		 * image of that point on a periodic parameter domain.  Adopt a newly
		 * refined image only when it is lift-equivalent and strictly nearer the
		 * exact interior branch.  This prevents a full-period final chord while
		 * leaving nonperiodic endpoint constraints untouched. */
		if (continuous_lift.IsValid() &&
			continuous_lift.DistanceTo(edge_point) <= pullback_tolerance &&
			(!required_endpoint || continuous_uv.DistanceTo(
			    points[adjacent_index]) < points[endpoint_index].DistanceTo(
			    points[adjacent_index])))
		    points[endpoint_index] = continuous_uv;
	    }
	    /* At a true surface pole the periodic parameter is undefined.  A
	     * closest-point solve can consequently return any longitude for the
	     * exact endpoint even though the adjacent, nonsingular edge samples
	     * converge on one unambiguous branch.  Leaving that arbitrary value in
	     * the pcurve creates a zero-area hook at the pole: the BREP remains
	     * structurally valid, but shaded-mesh repair can detach or omit the
	     * spherical cap.  Select the limiting branch of the already validated
	     * adjacent edge sample.  The candidate is accepted only when the
	     * surface advertises the boundary as singular and its 3-D lift still
	     * matches both the exact STEP edge endpoint and topology vertex within
	     * the established tolerance. */
	    for (int end = 0; end < 2; ++end) {
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		ON_3dPoint candidate = points[endpoint_index];
		int singular_direction = -1;
		int singular_side = -1;
		for (int direction = 0; direction < 2 && singular_direction < 0;
			direction++) {
		    const ON_Interval domain = surface->Domain(direction);
		    if (!domain.IsIncreasing())
			continue;
		    const double guard = std::max(ON_ZERO_TOLERANCE *
			kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    for (int side = 0; side < 2; ++side) {
			const int surface_side = direction == 0 ?
			    (side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
			if (fabs(candidate[direction] - domain[side]) <= guard &&
				surface->IsSingular(surface_side)) {
			    singular_direction = direction;
			    singular_side = side;
			    break;
			}
		    }
		}
		if (singular_direction < 0 ||
			!surface->IsClosed(1 - singular_direction))
		    continue;
		const int branch_direction = 1 - singular_direction;
		candidate[singular_direction] =
		    surface->Domain(singular_direction)[singular_side];
		candidate[branch_direction] =
		    points[adjacent_index][branch_direction];
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const int vertex_index = trim.m_vi[end];
		const ON_3dPoint lift = closed_surface_point_at(surface, candidate);
		if (lift.IsValid() && edge_point.IsValid() && vertex_index >= 0 &&
			vertex_index < brep->m_V.Count() &&
			lift.DistanceTo(edge_point) <= tolerance &&
			lift.DistanceTo(brep->m_V[vertex_index].point) <= tolerance)
		    points[endpoint_index] = candidate;
	    }
	    for (int point_index = 1; point_index < points.Count();) {
		if (points[point_index - 1].DistanceTo(points[point_index]) >
			ON_ZERO_TOLERANCE) {
		    ++point_index;
		    continue;
		}
		const int remove_index = point_index == points.Count() - 1 ?
		    point_index - 1 : point_index;
		points.Remove(remove_index);
		normalized_parameters.erase(normalized_parameters.begin() + remove_index);
		point_index = std::max(1, remove_index);
	    }
	    if (points.Count() < 2) {
		if (failure_reason)
		    *failure_reason = "continuous endpoint pullback collapsed the pcurve";
		continue;
	    }
	}

	/* Refine only the exact-edge pullback segments whose straight UV chord
	 * misses the edge.  Uniformly raising the global sample count performs
	 * poorly near surface singularities and wastes thousands of samples on
	 * already-flat portions of the curve. */
	std::set<std::pair<double, double> > singular_parameter_connectors;
	if (edge_driven) {
	    const size_t maximum_points = kMaximumAdaptivePullbackPoints;
	    bool replaced_required_endpoint_image[2] = {false, false};
	    /* Inserting a midpoint changes only the interval being split.  Advance
	     * monotonically after an interval passes; after a split, retain the
	     * current index to prove its left child and then its right child.  A
	     * restart from segment zero (even with an ordered cache of validated
	     * intervals) makes a 4096-point refinement quadratic in both scans and
	     * tree lookups.  No acceptance test changes: every resulting interval
	     * still passes the same three quarter-point lift checks and the complete
	     * candidate still receives the dense validation below. */
	    int segment = 0;
	    size_t refinement_iterations = 0;
	    while (valid && segment + 1 < points.Count()) {
		bool excessive_nonperiodic_branch = false;
		for (int endpoint = 0; endpoint < 2 &&
			excessive_nonperiodic_branch == false; ++endpoint) {
		    const ON_3dPoint &point = points[segment + endpoint];
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
		    const ON_Interval domain = surface->Domain(direction);
		    const double guard = std::max(ON_ZERO_TOLERANCE,
			    kPeriodicParameterSnapFraction *
				std::max(1.0, domain.Length()));
		    const bool outside_native =
			point[direction] < domain.Min() - guard ||
			point[direction] > domain.Max() + guard;
		    if (outside_native && periodic_crossing &&
			    !periodic_crossing->detected) {
			periodic_crossing->detected = true;
			periodic_crossing->trim_fraction = 0.5 *
			    (normalized_parameters[segment] +
			     normalized_parameters[segment + 1]);
			periodic_crossing->surface_direction = direction;
		    }
		    excessive_nonperiodic_branch =
			point[direction] < domain.Min() - domain.Length() - guard ||
			point[direction] > domain.Max() + domain.Length() + guard;
		    if (excessive_nonperiodic_branch) {
			std::ostringstream detail;
			detail << "closed nonperiodic surface pullback exceeded one "
			    << "unwrapped image in direction " << direction
			    << " at segment " << segment << " endpoint "
			    << endpoint << " (parameter " << point[direction]
			    << ", domain " << domain.Min() << ':' << domain.Max()
			    << ')';
			rejection = detail.str();
			break;
		    }
		}
	    }
	    if (excessive_nonperiodic_branch) {
		valid = false;
		break;
	    }
		if (wrapper && ((refinement_iterations++ & 31) == 0)) {
		    std::ostringstream detail;
		    detail << "trim=T" << trim.m_trim_index << " segment="
			<< segment << '/' << points.Count() - 1 << " points="
			<< points.Count() << '/' << maximum_points;
		    if (segment >= 0 && segment + 1 < points.Count())
			detail << " interval=" << normalized_parameters[segment]
			    << ':' << normalized_parameters[segment + 1]
			    << " uv=" << points[segment].x << ':'
			    << points[segment].y << "->" << points[segment + 1].x
			    << ':' << points[segment + 1].y;
		    wrapper->SetProgressDetail("refining exact edge pullback", 0,
			static_cast<uint64_t>(points.Count()),
			static_cast<uint64_t>(maximum_points), "points", detail.str());
		}
		if (brlcad::PullbackWorkCancelled()) {
		    if (failure_reason)
			*failure_reason =
			    "adaptive exact-edge pullback was cancelled";
		    return false;
		}
		bool split_segment = false;
		    for (int sub = 1; sub <= 3; ++sub) {
			const double fraction = static_cast<double>(sub) / 4.0;
			const double normalized = (1.0 - fraction) *
			    normalized_parameters[segment] + fraction *
			    normalized_parameters[segment + 1];
			const ON_3dPoint candidate_uv = (1.0 - fraction) * points[segment] +
			    fraction * points[segment + 1];
			const ON_3dPoint candidate_lift = surface->PointAt(
			    candidate_uv.x, candidate_uv.y);
			const double edge_parameter = edge->Domain().ParameterAt(
			    trim.m_bRev3d ? 1.0 - normalized : normalized);
			if (!candidate_lift.IsValid() || candidate_lift.DistanceTo(
				edge->PointAt(edge_parameter)) > tolerance) {
			    split_segment = true;
			    break;
			}
		    }
		    if (!split_segment) {
			++segment;
			continue;
		    }
		    bool collapsed_periodic_branch = false;
		    double rejected_periodic_shift_distance = DBL_MAX;
		    const double second_normalized =
			normalized_parameters[segment + 1];
		    const ON_3dPoint second_edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - second_normalized : second_normalized));
		    const bool preserve_required_end = required_end &&
			segment + 1 == points.Count() - 1;
		    for (int direction = 0; !preserve_required_end && direction < 2;
			    ++direction) {
			if (!surface->IsClosed(direction))
			    continue;
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted = points[segment + 1];
			shifted[direction] += std::round((points[segment][direction] -
			    shifted[direction]) / period) * period;
			if (fabs(shifted[direction] -
				points[segment + 1][direction]) <= ON_ZERO_TOLERANCE ||
				shifted.DistanceTo(points[segment]) >=
				points[segment + 1].DistanceTo(points[segment]))
			    continue;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted);
			double shifted_distance = shifted_lift.IsValid() &&
			    second_edge_point.IsValid() ?
			    shifted_lift.DistanceTo(second_edge_point) : DBL_MAX;
			if (shifted_distance > tolerance && second_edge_point.IsValid()) {
			    ON_3dPoint refined_shift = shifted;
			    double refined_distance = DBL_MAX;
				if (refine_surface_pullback_seeded(surface, second_edge_point,
					pullback_solver_tolerance(tolerance), refined_shift,
					&refined_distance, false, tolerance) &&
				    refined_shift.DistanceTo(points[segment]) <
					points[segment + 1].DistanceTo(points[segment])) {
				shifted = refined_shift;
				shifted_distance = refined_distance;
			    }
			}
			if (shifted_distance < rejected_periodic_shift_distance) {
			    rejected_periodic_shift_distance = shifted_distance;
			}
			if (shifted_distance <= tolerance) {
			    points[segment + 1] = shifted;
			    collapsed_periodic_branch = true;
			}
		    }
		    if (collapsed_periodic_branch) {
			/* Recheck this interval after selecting the nearer exact
			 * periodic image.  Its following interval has not yet been
			 * visited, so no accepted prefix is invalidated. */
			continue;
		    }
		    const double parameter_midpoint = 0.5 *
			(normalized_parameters[segment] +
			 normalized_parameters[segment + 1]);
		    if (!(parameter_midpoint > normalized_parameters[segment] &&
			    parameter_midpoint < normalized_parameters[segment + 1])) {
			/* A required loop endpoint can name a different periodic UV image
			 * of the same exact 3-D vertex.  Adaptive edge pullback approaches
			 * the continuous image from the interior, but eventually no double
			 * remains between its normalized parameter and 0 or 1.  At that
			 * machine-precision limit, retain the interior-side image only when
			 * its lift independently matches both the exact edge endpoint and
			 * topology vertex under a tighter tolerance.  The complete candidate
			 * is still densely validated below, and the adjacent-loop repair moves
			 * the neighboring endpoint to the same lift-equivalent image. */
			const bool closed_surface = surface->IsClosed(0) ||
			    surface->IsClosed(1);
			const bool collapsed_required_start = closed_surface &&
			    required_start && segment == 0;
			const bool collapsed_required_end = closed_surface &&
			    required_end && segment + 1 == points.Count() - 1;
			if (collapsed_required_start || collapsed_required_end) {
			    const int end = collapsed_required_start ? 0 : 1;
			    const int limit_index = collapsed_required_start ?
				segment + 1 : segment;
			    const ON_3dPoint limit_lift = closed_surface_point_at(
				surface, points[limit_index]);
			    const ON_3dPoint edge_endpoint = edge->PointAt(
				edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
			    const int vertex_index = trim.m_vi[end];
			    const double endpoint_tolerance = std::max(
				ON_ZERO_TOLERANCE * kNumericalToleranceScale,
				tolerance * 0.1);
			    if (limit_lift.IsValid() && edge_endpoint.IsValid() &&
				vertex_index >= 0 && vertex_index < brep->m_V.Count() &&
				limit_lift.DistanceTo(edge_endpoint) <= endpoint_tolerance &&
				limit_lift.DistanceTo(brep->m_V[vertex_index].point) <=
				    tolerance) {
				if (replaced_required_endpoint_image[end]) {
				    rejection = "adaptive exact-edge pullback repeatedly "
					"replaced the same required endpoint after exhausting "
					"representable parameter progress";
				    valid = false;
				    break;
				}
				replaced_required_endpoint_image[end] = true;
				if (collapsed_required_start) {
				    points.Remove(0);
				    normalized_parameters.erase(
					normalized_parameters.begin());
				    normalized_parameters.front() = 0.0;
				    segment = 0;
				} else {
				    points.Remove(points.Count() - 1);
				    normalized_parameters.pop_back();
				    normalized_parameters.back() = 1.0;
				    segment = std::max(0, segment - 1);
				}
				continue;
			    }
			}
			if (!valid)
			    break;
			const int varying_direction = fabs(points[segment + 1].y -
			    points[segment].y) > fabs(points[segment + 1].x -
			    points[segment].x) ? 1 : 0;
			const int fixed_direction = 1 - varying_direction;
			const ON_Interval fixed_domain = surface->Domain(fixed_direction);
			std::vector<double> fixed_candidates;
			fixed_candidates.reserve(134);
			const double fixed_first = points[segment][fixed_direction];
			const double fixed_second = points[segment + 1][fixed_direction];
			fixed_candidates.push_back(fixed_first);
			fixed_candidates.push_back(fixed_second);
			fixed_candidates.push_back(0.5 * (fixed_first + fixed_second));
			fixed_candidates.push_back(fixed_domain.Min());
			fixed_candidates.push_back(fixed_domain.Max());
			const double fixed_min = std::min(fixed_first, fixed_second);
			const double fixed_max = std::max(fixed_first, fixed_second);
			const double fixed_span = fixed_max - fixed_min;
			if (fixed_span > ON_ZERO_TOLERANCE) {
			    const double search_min = fixed_min - 4.0 * fixed_span;
			    const double search_max = fixed_max + 4.0 * fixed_span;
			    for (int search_sample = 0; search_sample <= 128;
				    ++search_sample)
				fixed_candidates.push_back(search_min +
				    (search_max - search_min) * search_sample / 128.0);
			}
			bool connected = false;
			double best_cost = DBL_MAX;
			ON_3dPoint best_start;
			ON_3dPoint best_end;
			/* A constant-parameter connector can represent the same 3-D edge
			 * while its UV endpoints differ only when that parameter direction
			 * has a genuinely collapsed surface side.  The former unconditional
			 * search evaluated up to 134 * 1025 expensive curve/surface pairs
			 * after an interval had already reached adjacent floating-point
			 * parameters, even on cylinders and other non-singular surfaces.
			 * OpenNURBS' singular-side classification is the required geometric
			 * proof; without it, reject the no-progress interval immediately. */
			const int first_singular_side = fixed_direction == 0 ? 3 : 0;
			const int second_singular_side = fixed_direction == 0 ? 1 : 2;
			const bool singular_connector_possible =
			    surface->IsSingular(first_singular_side) ||
			    surface->IsSingular(second_singular_side);
			for (std::vector<double>::const_iterator fixed_candidate =
				fixed_candidates.begin(); fixed_candidate !=
				fixed_candidates.end() && singular_connector_possible;
				++fixed_candidate) {
			    if (brlcad::PullbackWorkCancelled()) {
				if (failure_reason)
				    *failure_reason =
					"singular connector search was cancelled";
				return false;
			    }
			    ON_3dPoint connector_start = points[segment];
			    ON_3dPoint connector_end = points[segment + 1];
			    connector_start[fixed_direction] =
				*fixed_candidate;
			    connector_end[fixed_direction] =
				*fixed_candidate;
			    if (connector_start.DistanceTo(connector_end) <=
				    ON_ZERO_TOLERANCE)
				continue;
			    bool exact_connector = true;
			for (int connector_sample = 0;
				exact_connector &&
				connector_sample <= kDenseValidationSegments;
				++connector_sample) {
				if ((connector_sample & 63) == 0 &&
					brlcad::PullbackWorkCancelled()) {
				    if (failure_reason)
					*failure_reason =
					    "singular connector validation was cancelled";
				    return false;
				}
				const double fraction = static_cast<double>(connector_sample) /
				    kDenseValidationSegments;
				const double normalized = (1.0 - fraction) *
				    normalized_parameters[segment] + fraction *
				    normalized_parameters[segment + 1];
				const ON_3dPoint connector_uv = (1.0 - fraction) *
				    connector_start + fraction * connector_end;
				const ON_3dPoint connector_lift = closed_surface_point_at(
				    surface, connector_uv);
				const ON_3dPoint connector_edge = edge->PointAt(
				    edge->Domain().ParameterAt(trim.m_bRev3d ?
					1.0 - normalized : normalized));
				const double connector_tolerance =
				    (connector_sample == 0 ||
				     connector_sample == kDenseValidationSegments) ?
				    std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
					tolerance * 0.1) : tolerance;
				exact_connector = connector_lift.IsValid() &&
				    connector_edge.IsValid() && connector_lift.DistanceTo(
					connector_edge) <= connector_tolerance;
			    }
			    const double cost = fabs(*fixed_candidate -
				points[segment][fixed_direction]) +
				fabs(*fixed_candidate -
				points[segment + 1][fixed_direction]);
			    if (exact_connector && cost < best_cost) {
				connected = true;
				best_cost = cost;
				best_start = connector_start;
				best_end = connector_end;
			    }
			}
			if (connected) {
			    points[segment] = best_start;
			    points[segment + 1] = best_end;
			    singular_parameter_connectors.insert(std::make_pair(
				normalized_parameters[segment],
				normalized_parameters[segment + 1]));
			    /* The complete connector was densely proven above. */
			    ++segment;
			    continue;
			}
			if (periodic_crossing && !periodic_crossing->detected) {
			    for (int direction = 0; direction < 2; ++direction) {
				if (!surface->IsClosed(direction))
				    continue;
				const double period = surface->Domain(direction).Length();
				if (!(period > ON_ZERO_TOLERANCE) || fabs(
					points[segment + 1][direction] -
					points[segment][direction]) <= 0.5 * period)
				    continue;
				periodic_crossing->detected = true;
				periodic_crossing->trim_fraction = 0.5 *
				    (normalized_parameters[segment] +
				     normalized_parameters[segment + 1]);
				periodic_crossing->surface_direction = direction;
				break;
			    }
			}
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
			rejection = "adaptive exact-edge pullback made no parameter progress at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) + ", domains " +
			    std::to_string(surface->Domain(0).Min()) + ":" +
			    std::to_string(surface->Domain(0).Max()) + "," +
			    std::to_string(surface->Domain(1).Min()) + ":" +
			    std::to_string(surface->Domain(1).Max()) + ", closed " +
			    std::to_string(surface->IsClosed(0) ? 1 : 0) +
			    std::to_string(surface->IsClosed(1) ? 1 : 0) +
			    ", periodic " +
			    std::to_string(surface->IsPeriodic(0) ? 1 : 0) +
			    std::to_string(surface->IsPeriodic(1) ? 1 : 0) +
			    ", singular " +
			    std::to_string(surface->IsSingular(0) ? 1 : 0) +
			    std::to_string(surface->IsSingular(1) ? 1 : 0) +
			    std::to_string(surface->IsSingular(2) ? 1 : 0) +
			    std::to_string(surface->IsSingular(3) ? 1 : 0) +
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }
		    if (static_cast<size_t>(points.Count()) >= maximum_points) {
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
			rejection = "adaptive exact-edge pullback sample budget exceeded at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) +
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }

		    const double normalized = parameter_midpoint;
		    const double edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		    const double pullback_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		    const double solver_tolerance = pullback_solver_tolerance(
			pullback_tolerance);
		    const ON_3dPoint branch_reference = 0.5 *
			(points[segment] + points[segment + 1]);
		    ON_3dPoint uv = branch_reference;
		    bool confine_nonperiodic_closed_surface = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			confine_nonperiodic_closed_surface = true;
			const ON_Interval direction_domain =
			    surface->Domain(direction);
			const double direction_period = direction_domain.Length();
			if (!(direction_period > ON_ZERO_TOLERANCE))
			    continue;
			double wrapped = fmod(uv[direction] -
			    direction_domain.Min(), direction_period);
			if (wrapped < 0.0)
			    wrapped += direction_period;
			uv[direction] = direction_domain.Min() + wrapped;
		    }
		    double pullback_distance = DBL_MAX;
		    if (!refine_surface_pullback_seeded(surface, edge_point,
			    solver_tolerance, uv, &pullback_distance,
			    confine_nonperiodic_closed_surface ||
				!has_unwrapped_closed_parameter(surface, uv),
			    pullback_tolerance)) {
			ON_2dPoint pulled_uv;
			ON_3dPoint pulled_lift;
			if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
				pulled_lift, pullback_distance, 0,
				solver_tolerance,
				pullback_tolerance) || pullback_distance > pullback_tolerance) {
			    rejection = "adaptive exact-edge pullback failed";
			    valid = false;
			    break;
			}
			uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    }
		    const ON_3dPoint source_uv = trim.PointAt(
			trim_domain.ParameterAt(normalized));
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			const double direction_period =
			    surface->Domain(direction).Length();
			if (!(direction_period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += round((source_uv[direction] -
			    uv[direction]) / direction_period) * direction_period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <=
				    pullback_tolerance)
			    uv = shifted_uv;
		    }
		    bool excessive_period_drift = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			const double direction_period =
			    surface->Domain(direction).Length();
			excessive_period_drift = excessive_period_drift ||
			    (direction_period > ON_ZERO_TOLERANCE &&
			     fabs(uv[direction] - source_uv[direction]) >
				 kMaximumSeededPullbackPeriodDrift * direction_period);
		    }
		    if (excessive_period_drift) {
			ON_2dPoint native_uv;
			ON_3dPoint native_lift;
			double native_distance = DBL_MAX;
			if (pullback_context.SurfaceClosestPoint(surface, edge_point,
				native_uv, native_lift, native_distance, 0,
				solver_tolerance, pullback_tolerance) &&
			    native_distance <= pullback_tolerance)
			    uv.Set(native_uv.x, native_uv.y, 0.0);
		    }
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((branch_reference[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			uv = shifted_uv;
		    }
		    normalize_closed_surface_parameter(surface, edge_point,
			pullback_tolerance, uv);
		    points.Insert(segment + 1, uv);
		    normalized_parameters.insert(normalized_parameters.begin() +
			segment + 1, normalized);
		    /* Prove the newly created left child before advancing. */
	    }
	    if (!valid) {
		if (failure_reason)
		    *failure_reason = rejection;
		continue;
	    }
	}
	/* A fitted closed pcurve, or an open pcurve whose endpoint was constrained
	 * to close a periodic loop, can have a poor one-sided derivative at that
	 * algebraic endpoint even when its interior follows the edge correctly.
	 * Map the exact 3D edge tangent into the surface parameter plane and add a
	 * short endpoint chord without moving an already validated interior sample.
	 * The validation below still
	 * bounds both the displacement from the source pcurve and the distance to
	 * the exact edge, so this cannot bridge an out-of-tolerance gap. */
	const bool closed_topology = trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	bool ill_conditioned_endpoint[2] = {false, false};
	for (int end = 0; valid && end < 2; ++end) {
	    if (!closed_topology && !(end == 0 ? required_start : required_end))
		continue;
	    const int current_segment_count = points.Count() - 1;
	    const int endpoint_index = end == 0 ? 0 : current_segment_count;
	    int adjacent_index = end == 0 ? 1 : current_segment_count - 1;
	    const ON_3dPoint endpoint_uv = points[endpoint_index];
	    /* Periodic endpoint limiting can produce several parameter samples at
	     * the same representable UV before the curve makes geometric progress.
	     * The cleanup below removes those duplicates, but the endpoint tangent
	     * proof must likewise use the first distinct interior sample instead
	     * of treating its initial zero-length chord as a failed pullback. */
	    const int adjacent_step = end == 0 ? 1 : -1;
	    while (adjacent_index > 0 &&
		    adjacent_index < current_segment_count &&
		    points[adjacent_index].IsCoincident(endpoint_uv))
		adjacent_index += adjacent_step;
	    if (adjacent_index < 0 ||
		    adjacent_index > current_segment_count ||
		    adjacent_index == endpoint_index ||
		    points[adjacent_index].IsCoincident(endpoint_uv)) {
		rejection =
		    "endpoint limiting collapsed every adjacent parameter sample";
		valid = false;
		break;
	    }
	    ON_3dPoint lifted;
	    ON_3dVector du, dv;
	    if (!closed_surface_ev1der(surface, endpoint_uv, lifted, du, dv)) {
		rejection = "endpoint surface derivative evaluation failed";
		valid = false;
		break;
	    }
	    ON_3dVector target = edge->TangentAt(edge->Domain()[
		trim.m_bRev3d ? 1 - end : end]);
	    if (trim.m_bRev3d)
		target.Reverse();
	    if (!target.Unitize()) {
		rejection = "endpoint edge tangent evaluation failed";
		valid = false;
		break;
	    }
	    const ON_3dVector current_uv_tangent = end == 0 ?
		points[adjacent_index] - endpoint_uv :
		endpoint_uv - points[adjacent_index];
	    ON_3dVector current_lifted_tangent = current_uv_tangent.x * du +
		current_uv_tangent.y * dv;
	    if (current_lifted_tangent.Unitize()) {
		const double current_alignment = current_lifted_tangent * target;
		if (current_alignment >= 0.0)
		    continue;
		/* OpenNURBS requires a nonnegative endpoint tangent alignment for a
		 * closed edge use.  Rotate only far enough to cross that sign boundary;
		 * replacing a nearly perpendicular supplied tangent with the complete
		 * edge tangent can move an otherwise exact pcurve beyond tolerance. */
		const double positive_margin = std::max(1.0e-6,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		target = current_lifted_tangent +
		    (-current_alignment + positive_margin) * target;
		if (!target.Unitize()) {
		    rejection = "endpoint tangent correction failed";
		    valid = false;
		    break;
		}
	    }
	    const double a = du * du;
	    const double b = du * dv;
	    const double c = dv * dv;
	    const double r0 = du * target;
	    const double r1 = dv * target;
	    const double determinant = a * c - b * b;
	    const double numerical_floor = ON_ZERO_TOLERANCE *
		std::max(1.0, a * c);
	    if (fabs(determinant) <= numerical_floor) {
		/* Some analytic revolution surfaces do not report their apex via
		 * IsAtSingularity(), but the rank-deficient Jacobian proves that an
		 * endpoint parameter tangent is not geometrically meaningful there.
		 * Preserve the exact endpoint and rely on the dense interior direction
		 * proof outside the guarded endpoint neighborhood. */
		ill_conditioned_endpoint[end] = true;
		continue;
	    }
	    ON_3dVector uv_tangent((c * r0 - b * r1) / determinant,
		(a * r1 - b * r0) / determinant, 0.0);
	    const ON_3dPoint adjacent_lift = closed_surface_point_at(surface,
		points[adjacent_index]);
	    const double chord_length = lifted.DistanceTo(adjacent_lift);
	    const ON_3dVector lifted_uv_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    const double tangent_scale = lifted_uv_tangent.Length();
	    if (!adjacent_lift.IsValid() || chord_length <= ON_ZERO_TOLERANCE ||
		    tangent_scale <= ON_ZERO_TOLERANCE) {
		rejection = "degenerate endpoint parameter tangent";
		valid = false;
		break;
	    }
	    uv_tangent *= 0.25 * chord_length / tangent_scale;
	    const ON_3dPoint tangent_point = end == 0 ?
		endpoint_uv + uv_tangent : endpoint_uv - uv_tangent;
	    const double tangent_parameter = end == 0 ?
		0.75 * normalized_parameters[endpoint_index] +
		    0.25 * normalized_parameters[adjacent_index] :
		0.25 * normalized_parameters[adjacent_index] +
		    0.75 * normalized_parameters[endpoint_index];
	    const int insertion_index = end == 0 ? 1 : endpoint_index;
	    points.Insert(insertion_index, tangent_point);
		    normalized_parameters.insert(normalized_parameters.begin() +
			insertion_index, tangent_parameter);
		}
		/* Periodic endpoint limiting can consume several successively refined
		 * samples whose normalized parameters differ but whose UV values have
		 * reached the same machine-precision limit.  Endpoint-tangent insertion
		 * may expose that duplicate pair.  Remove only zero-length consecutive
		 * samples, retaining a constrained algebraic endpoint when it is one of
		 * the pair; all remaining segments undergo the full validation below. */
		for (int point_index = 1; point_index < points.Count();) {
		    if (!points[point_index - 1].IsCoincident(
			    points[point_index])) {
			++point_index;
			continue;
		    }
		    const int remove_index = point_index == points.Count() - 1 ?
			point_index - 1 : point_index;
		    points.Remove(remove_index);
		    normalized_parameters.erase(normalized_parameters.begin() +
			remove_index);
		    point_index = std::max(1, remove_index);
		}
		if (points.Count() < 2) {
		    rejection = "periodic endpoint limiting collapsed the pcurve";
		    valid = false;
		}
		if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " segments";
	    continue;
	}
	const int candidate_segment_count = points.Count() - 1;

	/* Validate both geometry and direction at every new vertex and at interior
	 * points of every segment.  Comparison with the original lift bounds the
	 * repair itself; comparison with the edge verifies that no narrow invalid
	 * source deviation can hide exactly at a sampling vertex.  Surface and
	 * curve evaluation dominates this proof and each sample is independent, so
	 * let an otherwise idle geometry helper evaluate samples in parallel.  All
	 * acceptance decisions remain in deterministic parameter order below. */
	struct DenseValidationSample {
	    double normalized = 0.0;
	    ON_3dPoint candidate_uv = ON_3dPoint::UnsetPoint;
	    ON_3dPoint candidate_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint original_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint edge_point = ON_3dPoint::UnsetPoint;
	    ON_3dVector du = ON_3dVector::UnsetVector;
	    ON_3dVector dv = ON_3dVector::UnsetVector;
	    bool evaluated = false;
	    bool edge_locus_proven = false;
	};
	const size_t dense_sample_count =
	    static_cast<size_t>(candidate_segment_count) * 4;
	std::vector<DenseValidationSample> dense_samples(dense_sample_count);
	const auto evaluate_dense_sample = [&](size_t dense_index) {
	    if (brlcad::PullbackWorkCancelled())
		return;
	    const int segment = static_cast<int>(dense_index / 4);
	    const int sub = static_cast<int>(dense_index % 4);
	    const double fraction = static_cast<double>(sub) / 4.0;
	    DenseValidationSample &sample = dense_samples[dense_index];
	    sample.normalized = (1.0 - fraction) *
		normalized_parameters[segment] + fraction *
		normalized_parameters[segment + 1];
	    sample.candidate_uv = (1.0 - fraction) * points[segment] +
		fraction * points[segment + 1];
	    if (!closed_surface_ev1der(surface, sample.candidate_uv,
		    sample.candidate_lift, sample.du, sample.dv))
		return;
	    const ON_3dPoint original_uv = trim.PointAt(
		trim_domain.ParameterAt(sample.normalized));
	    sample.original_lift = closed_surface_point_at(surface, original_uv);
	    sample.edge_point = edge->PointAt(edge->Domain().ParameterAt(
		trim.m_bRev3d ? 1.0 - sample.normalized : sample.normalized));
	    sample.evaluated = sample.candidate_lift.IsValid() &&
		sample.original_lift.IsValid() && sample.edge_point.IsValid();
	};
	if (wrapper)
	    wrapper->ParallelForGeometry(dense_sample_count, evaluate_dense_sample);
	else
	    for (size_t dense_index = 0; dense_index < dense_sample_count;
		    ++dense_index)
		evaluate_dense_sample(dense_index);
	if (brlcad::PullbackWorkCancelled()) {
	    if (failure_reason)
		*failure_reason = "dense regenerated-pcurve validation was cancelled";
	    return false;
	}
	/* Source-driven regeneration preserves the supplied pcurve's parameter
	 * speed, which need not match the independently parameterized STEP edge.
	 * The old validation called ON_NurbsCurve_GetClosestPoint separately for
	 * every mismatched dense sample.  On a long spline edge that repeatedly
	 * rebuilt and globally searched the same knot spans thousands of times
	 * after sampling had visibly reached 1024/1024.
	 *
	 * Prove the same curve-locus condition as one ordered batch.  The shared
	 * evaluator brackets every exact NURBS span once, uses conservative span
	 * boxes (and an R-tree for large curves), and reuses the preceding accepted
	 * span as the next seed.  No tolerance or sampling decision changes. */
	if (!edge_driven) {
	    std::vector<ON_3dPoint> locus_points;
	    std::vector<size_t> locus_dense_indices;
	    locus_points.reserve(dense_samples.size());
	    locus_dense_indices.reserve(dense_samples.size());
	    for (size_t dense_index = 0; dense_index < dense_samples.size();
		    ++dense_index) {
		DenseValidationSample &sample = dense_samples[dense_index];
		if (!sample.evaluated)
		    continue;
		const double direct_distance =
		    sample.candidate_lift.DistanceTo(sample.edge_point);
		if (direct_distance <= tolerance) {
		    sample.edge_locus_proven = true;
		    continue;
		}
		locus_points.push_back(sample.candidate_lift);
		locus_dense_indices.push_back(dense_index);
	    }
	    if (!locus_points.empty()) {
		if (wrapper) {
		    std::ostringstream detail;
		    detail << "trim=T" << trim.m_trim_index << " points="
			<< locus_points.size() << " edge=STEP#"
			<< edge->m_edge_user.i << " edge-degree="
			<< edge_nurbs.Degree() << " edge-cvs="
			<< edge_nurbs.CVCount() << " edge-spans="
			<< edge_nurbs.SpanCount();
		    wrapper->SetProgressDetail(
			"validating regenerated pcurve against exact edge locus",
			0, 0, static_cast<uint64_t>(locus_points.size()),
			"points", detail.str());
		}
		size_t rejected_index = 0;
		double rejected_distance = DBL_MAX;
		const std::function<void(size_t, size_t, const std::string &)>
		    locus_progress =
		    [wrapper, &trim, edge, &edge_nurbs](size_t completed,
			    size_t total, const std::string &stage) {
			if (!wrapper)
			    return;
			std::ostringstream detail;
			detail << "trim=T" << trim.m_trim_index
			    << " edge=STEP#" << edge->m_edge_user.i
			    << " edge-degree=" << edge_nurbs.Degree()
			    << " edge-cvs=" << edge_nurbs.CVCount()
			    << " edge-spans=" << edge_nurbs.SpanCount();
			if (!stage.empty())
			    detail << " stage=\"" << stage << '"';
			wrapper->SetProgressDetail(
			    "validating regenerated pcurve against exact edge locus",
			    0, static_cast<uint64_t>(completed),
			    static_cast<uint64_t>(total), "points",
			    detail.str());
		    };
		if (!step_curve_locus_contains_points(&edge_nurbs,
			locus_points.data(), locus_points.size(), tolerance,
			&rejected_index, &rejected_distance,
			locus_progress)) {
		    const size_t dense_index = rejected_index <
			locus_dense_indices.size() ?
			locus_dense_indices[rejected_index] : 0;
		    const double normalized = dense_index < dense_samples.size() ?
			dense_samples[dense_index].normalized : 0.0;
		    rejection = "regenerated lift exceeded edge tolerance "
			"(batched locus distance " +
			std::to_string(rejected_distance) + ", tolerance " +
			std::to_string(tolerance) + ", normalized " +
			std::to_string(normalized) + ")";
		    valid = false;
		} else {
		    for (std::vector<size_t>::const_iterator dense_index =
			    locus_dense_indices.begin();
			    dense_index != locus_dense_indices.end();
			    ++dense_index)
			dense_samples[*dense_index].edge_locus_proven = true;
		}
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " +
		    std::to_string(segment_count) + " initial segments (" +
		    std::to_string(candidate_segment_count) +
		    " final segments)";
	    continue;
	}
	for (int segment = 0; valid && segment < candidate_segment_count; ++segment) {
	    const ON_3dVector uv_tangent = points[segment + 1] - points[segment];
	    if (uv_tangent.Length() <= ON_ZERO_TOLERANCE) {
		rejection = "zero-length regenerated pcurve segment " +
		    std::to_string(segment) + " (normalized " +
		    std::to_string(normalized_parameters[segment]) + ":" +
		    std::to_string(normalized_parameters[segment + 1]) + ", uv " +
		    std::to_string(points[segment].x) + ":" +
		    std::to_string(points[segment].y) + " -> " +
		    std::to_string(points[segment + 1].x) + ":" +
		    std::to_string(points[segment + 1].y) + ")";
		valid = false;
		break;
	    }
	    for (int sub = 0; sub <= 3; ++sub) {
		const DenseValidationSample &sample = dense_samples[
		    static_cast<size_t>(segment) * 4 + sub];
		const double normalized = sample.normalized;
		const ON_3dPoint &candidate_uv = sample.candidate_uv;
		const ON_3dPoint &candidate_lift = sample.candidate_lift;
		const ON_3dPoint &original_lift = sample.original_lift;
		const ON_3dVector &du = sample.du;
		const ON_3dVector &dv = sample.dv;
		if (!sample.evaluated) {
		    rejection = "surface lift failed";
		    valid = false;
		    break;
		}
		const double source_lift_distance =
		    candidate_lift.DistanceTo(original_lift);
		if (!edge_driven && source_lift_distance > tolerance) {
		    rejection = "regenerated lift exceeded source pcurve tolerance"
			" (distance " + std::to_string(source_lift_distance) +
			", tolerance " + std::to_string(tolerance) +
			", normalized " + std::to_string(normalized) +
			", segment uv " + std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ")";
		    valid = false;
		    break;
		}
		double edge_parameter = 0.0;
		double edge_distance = DBL_MAX;
		edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		edge_distance = candidate_lift.DistanceTo(sample.edge_point);
		bool edge_proxy_parameter = true;
		bool edge_locus_proven = sample.edge_locus_proven ||
		    edge_distance <= tolerance;
		/* A trim and its 3-D edge describe the same directed locus, but are not
		 * required to advance at the same normalized parameter speed.  The
		 * regenerated polyline changes that speed locally, so a synchronized
		 * comparison can report a false geometric gap.  A synchronized point
		 * already inside the acceptance tolerance is also a constructive proof
		 * of edge-locus membership, however, and the global NURBS optimizer
		 * cannot change that decision.  Run the expensive closest-point solve
		 * only when the direct proof fails; the tangent test below still proves
		 * traversal direction. */
		if (!edge_locus_proven && edge_driven) {
		    double closest_parameter = 0.0;
		    if (ON_NurbsCurve_GetClosestPoint(&closest_parameter, &edge_nurbs,
			    candidate_lift)) {
			const double closest_distance = candidate_lift.DistanceTo(
			    edge_nurbs.PointAt(closest_parameter));
			if (closest_distance < edge_distance) {
			    edge_parameter = closest_parameter;
			    edge_distance = closest_distance;
			    edge_proxy_parameter = false;
			}
		    }
		    edge_locus_proven = edge_distance <= tolerance;
		}
		if (!edge_locus_proven) {
		    double original_parameter = 0.0;
		    double original_edge_distance = ON_UNSET_VALUE;
		    if (ON_NurbsCurve_GetClosestPoint(&original_parameter, &edge_nurbs,
			    original_lift))
		original_edge_distance = original_lift.DistanceTo(
			edge_nurbs.PointAt(original_parameter));
		    rejection = "regenerated lift exceeded edge tolerance (distance " +
			std::to_string(edge_distance) + ", source " +
			std::to_string(original_edge_distance) + ", tolerance " +
			std::to_string(tolerance) + ", normalized " +
			std::to_string(normalized) + ", segment uv " +
			std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ", candidate uv " +
			std::to_string(candidate_uv.x) + ":" +
			std::to_string(candidate_uv.y) + ")";
		    valid = false;
		    break;
		}
		if (singular_parameter_connectors.find(std::make_pair(
			normalized_parameters[segment],
			normalized_parameters[segment + 1])) !=
			singular_parameter_connectors.end())
		    continue;
		if (normalized <= kEndpointDirectionGuardFraction ||
			normalized >= 1.0 - kEndpointDirectionGuardFraction)
		    continue;
		ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
		if (!edge_driven) {
		    ON_3dPoint source_uv, source_lift;
		    ON_3dVector source_uv_tangent, source_du, source_dv;
		    const double source_parameter = trim_domain.ParameterAt(normalized);
		    if (!trim.Ev1Der(source_parameter, source_uv, source_uv_tangent) ||
			    !closed_surface_ev1der(surface, source_uv, source_lift,
				source_du, source_dv)) {
			rejection = "source pcurve tangent evaluation failed";
			valid = false;
			break;
		    }
		    ON_3dVector source_tangent = source_uv_tangent.x * source_du +
			source_uv_tangent.y * source_dv;
		    if (!lifted_tangent.Unitize() || !source_tangent.Unitize() ||
			    lifted_tangent * source_tangent <= 0.0) {
			rejection = "regenerated interior direction disagreed with source pcurve";
			valid = false;
			break;
		    }
		    continue;
		}
		ON_3dVector edge_tangent = edge_proxy_parameter ?
		    edge->TangentAt(edge_parameter) : edge_nurbs.TangentAt(edge_parameter);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		    rejection = "regenerated tangent evaluation failed";
		    valid = false;
		    break;
		}
		double alignment = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d)
		    alignment = -alignment;
		if (alignment <= 0.0) {
		    rejection = "regenerated interior direction disagreed with edge"
			" (segment " + std::to_string(segment) + ", normalized " +
			std::to_string(normalized) + ", alignment " +
			std::to_string(alignment) + ", reversed " +
			std::to_string(trim.m_bRev3d ? 1 : 0) + ")";
		    valid = false;
		    break;
		}
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    continue;
	}

	ON_SimpleArray<double> candidate_parameters;
	candidate_parameters.Reserve(static_cast<int>(normalized_parameters.size()));
	for (std::vector<double>::const_iterator normalized =
		normalized_parameters.begin(); normalized != normalized_parameters.end();
	     ++normalized)
	    candidate_parameters.Append(trim_domain.ParameterAt(*normalized));
	ON_PolylineCurve *candidate = new ON_PolylineCurve(points,
	    candidate_parameters);
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    delete candidate;
	    /* Endpoint limiting and duplicate removal can leave a valid UV locus
	     * with inherited edge parameters that are no longer strictly
	     * increasing at machine precision.  Reparameterize that unchanged
	     * locus monotonically over the authoritative trim domain.  The dense
	     * surface/edge and directed endpoint validation below still decides
	     * whether this representation is admissible. */
	    candidate = new ON_PolylineCurve(points);
	    if (!candidate->ChangeDimension(2) ||
		    !candidate->SetDomain(trim_domain.Min(),
			trim_domain.Max()) ||
		    !candidate->IsValid()) {
		if (failure_reason)
		    *failure_reason =
			"regenerated polyline locus or parameterization is invalid";
		delete candidate;
		continue;
	    }
	}
	if (trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "regenerated open-topology pcurve remained closed";
	    delete candidate;
	    continue;
	}
	const ON_Interval candidate_domain = candidate->Domain();
	for (int end = 0; valid && end < 2; ++end) {
	    ON_3dPoint uv, lifted_point;
	    ON_3dVector uv_tangent, du, dv;
	    if (!candidate->Ev1Der(candidate_domain[end], uv, uv_tangent) ||
		    !closed_surface_ev1der(surface, uv, lifted_point, du, dv)) {
		rejection = "regenerated endpoint evaluation failed";
		valid = false;
		break;
	    }
	    const int vertex_index = trim.m_vi[end];
	    const ON_3dPoint edge_endpoint = edge->PointAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!edge_endpoint.IsValid() ||
		    lifted_point.DistanceTo(edge_endpoint) > tolerance ||
		    vertex_index < 0 || vertex_index >= brep->m_V.Count() ||
		    lifted_point.DistanceTo(brep->m_V[vertex_index].point) > tolerance) {
		rejection = "regenerated endpoint exceeded exact topology tolerance";
		valid = false;
		break;
	    }
	    /* At a collapsed surface boundary the endpoint lift is exact, but its
	     * differential is not unique.  Interior samples above already prove
	     * traversal direction outside the guarded pole neighborhood. */
	    const ON_3dPoint native_uv = closed_surface_native_parameter(surface,
		uv);
	    if ((native_uv.IsValid() && surface->IsAtSingularity(native_uv.x,
		    native_uv.y, false)) ||
		    ill_conditioned_endpoint[end])
		continue;
	    ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    ON_3dVector edge_tangent = edge->TangentAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		rejection = "regenerated endpoint tangent evaluation failed";
		valid = false;
		break;
	    }
	    double alignment = lifted_tangent * edge_tangent;
	    if (trim.m_bRev3d)
		alignment = -alignment;
	    if (alignment < 0.0) {
		rejection = "regenerated endpoint direction disagreed with edge "
		    "(endpoint " + std::to_string(end) + ", alignment " +
		    std::to_string(alignment) + ", uv " + std::to_string(uv.x) +
		    ":" + std::to_string(uv.y) + ", singular " +
		    std::to_string(native_uv.IsValid() && surface->IsAtSingularity(
			native_uv.x, native_uv.y, false) ?
			1 : 0) + ")";
		valid = false;
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    delete candidate;
	    continue;
	}
	/* Installing an open, full-period pcurve into a one-trim closed-topology
	 * loop leaves that loop open in Euclidean UV and is structurally invalid in
	 * OpenNURBS, even though the endpoint lifts coincide on the closed surface.
	 * Periodic-band and pole-cut builders request the generated curve through
	 * generated_curve and install the required paired seam topology atomically;
	 * an ordinary in-place orientation repair has no authority to do so. */
	const ON_BrepLoop *owner_loop = trim.Loop();
	if (!generated_curve && trim.m_vi[0] == trim.m_vi[1] && owner_loop &&
		owner_loop->TrimCount() == 1 && !candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "regenerated standalone closed-topology pcurve "
		    "requires explicit periodic seam topology";
	    delete candidate;
	    continue;
	}
	if (generated_curve) {
	    *generated_curve = candidate;
	    return true;
	}
	const int c2_index = brep->AddTrimCurve(candidate);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index))
	    return false;
	const ON_Interval proxy_domain = trim.ProxyCurveDomain();
	trim.m_iso = surface->IsIsoparametric(*candidate, &proxy_domain);
	return true;
    }
    if (prefer_edge_driven && edge_driven && failure_reason)
	edge_driven_failure = *failure_reason;
    }
    if (prefer_edge_driven && failure_reason && !edge_driven_failure.empty() &&
	    *failure_reason != edge_driven_failure)
	*failure_reason = "exact-edge mode: " + edge_driven_failure +
	    "; supplied-pcurve mode: " + *failure_reason;
    return false;
}


static bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || loop_index < 0 ||
	    loop_index >= brep->m_L.Count())
	return false;
    const ON_BrepLoop &source_loop = brep->m_L[loop_index];
    const ON_BrepFace *source_face = source_loop.Face();
    const ON_Surface *source_surface = source_face ?
	source_face->SurfaceOf() : NULL;
    if (!source_surface || !source_surface->IsClosed(closed_direction) ||
	    source_loop.TrimCount() < 2)
	return false;
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = source_surface->Domain(closed_direction);
    if (!closed_domain.IsIncreasing())
	return false;
    const double period = closed_domain.Length();

    /* Most segmented periodic boundaries already have an exact pcurve; only
     * its integral-period image is inconsistent with the neighboring trim.
     * Re-pulling such a curve point by point is both unnecessary and very
     * expensive on rational spline surfaces.  Translate and, when required,
     * snap its constrained endpoints in UV, then accept it only if dense
     * parameter-corresponding samples still follow the authoritative 3-D
     * edge within topology tolerance.  A failed proof falls through to the
     * general edge-driven pullback below. */
    const auto translated_exact_pcurve = [surface = source_surface,
	    closed_direction, parameter_tolerance](ON_BrepTrim &trim,
	    const ON_BrepEdge &edge,
	    double tolerance, const ON_3dPoint &required_start,
	    const ON_3dPoint *required_end, ON_Curve **candidate_out,
	    std::string *failure_out) {
	if (candidate_out) *candidate_out = NULL;
	if (failure_out) failure_out->clear();
	const auto reject = [failure_out](const char *reason) {
	    if (failure_out) *failure_out = reason;
	    return false;
	};
	std::unique_ptr<ON_Curve> candidate(trim.DuplicateCurve());
	if (!candidate || !candidate->Domain().IsIncreasing() ||
		!edge.Domain().IsIncreasing())
	    return reject("invalid trim or edge domain");
	/* DuplicateCurve() may retain an ON_CurveProxy.  Materialize its exact
	 * NURBS form before applying a private period translation or endpoint
	 * edit; transforming a proxy can fail or mutate its shared target. */
	ON_NurbsCurve detached_candidate;
	if (!candidate->GetNurbForm(detached_candidate) ||
		!detached_candidate.IsValid())
	    return reject("could not materialize the trim pcurve");
	candidate.reset(new ON_NurbsCurve(detached_candidate));
	ON_Xform translation(ON_Xform::IdentityTransformation);
	bool translated = false;
	const ON_3dPoint original_start = candidate->PointAtStart();
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double direction_period = surface->Domain(direction).Length();
	    if (!(direction_period > ON_ZERO_TOLERANCE))
		continue;
	    const double shift = round((required_start[direction] -
		original_start[direction]) / direction_period) * direction_period;
	    if (fabs(shift) <= ON_ZERO_TOLERANCE)
		continue;
	    translation.m_xform[direction][3] = shift;
	    translated = true;
	}
	if (translated && !candidate->Transform(translation))
	    return reject("could not apply an integral-period translation");
	if (!candidate->ChangeDimension(2) || !candidate->IsValid())
	    return reject("translated trim pcurve was invalid");
	const ON_Interval edge_domain = edge.Domain();
	const ON_3dPoint required_start_lift = surface->PointAt(
	    required_start.x, required_start.y);
	const ON_3dPoint edge_start = edge.PointAt(
	    edge_domain[trim.m_bRev3d ? 1 : 0]);
	if (!required_start.IsValid() || !required_start_lift.IsValid() ||
		!edge_start.IsValid() ||
		required_start_lift.DistanceTo(edge_start) > tolerance)
	    return reject("required start did not lift to the edge endpoint");
	const ON_3dPoint candidate_start = candidate->PointAtStart();
	if (!candidate_start.IsValid())
	    return reject("could not constrain the pcurve start");
	ON_3dPoint candidate_end = candidate->PointAtEnd();
	if (!candidate_end.IsValid())
	    return reject("could not evaluate the pcurve end");
	const bool prepend_connector = candidate_start.DistanceTo(required_start) >
	    ON_ZERO_TOLERANCE;
	bool append_connector = false;
	if (required_end) {
	    const ON_3dPoint required_end_lift = surface->PointAt(
		required_end->x, required_end->y);
	    const ON_3dPoint edge_end = edge.PointAt(
		edge_domain[trim.m_bRev3d ? 0 : 1]);
	    if (!required_end->IsValid() || !required_end_lift.IsValid() ||
		    !edge_end.IsValid() ||
		    required_end_lift.DistanceTo(edge_end) > tolerance)
		return reject("required end did not lift to the edge endpoint");
	    append_connector = candidate_end.DistanceTo(*required_end) >
		ON_ZERO_TOLERANCE;
	}
	if (prepend_connector || append_connector) {
	    std::unique_ptr<ON_PolyCurve> joined(new ON_PolyCurve());
	    joined->Reserve(1 + (prepend_connector ? 1 : 0) +
		(append_connector ? 1 : 0));
	    if (prepend_connector) {
		ON_LineCurve *connector = new ON_LineCurve(required_start,
		    candidate_start);
		if (!connector->ChangeDimension(2) || !connector->IsValid() ||
			!joined->Append(connector)) {
		    delete connector;
		    return reject("could not prepend an exact endpoint connector");
		}
	    }
	    ON_Curve *middle = candidate.release();
	    if (!joined->Append(middle)) {
		delete middle;
		return reject("could not append the existing pcurve segment");
	    }
	    if (append_connector) {
		ON_LineCurve *connector = new ON_LineCurve(candidate_end,
		    *required_end);
		if (!connector->ChangeDimension(2) || !connector->IsValid() ||
			!joined->Append(connector)) {
		    delete connector;
		    return reject("could not append an exact endpoint connector");
		}
	    }
	    if (!joined->SetDomain(trim.Domain().Min(), trim.Domain().Max()) ||
		    !joined->IsValid())
		return reject("joined pcurve connector was invalid");
	    candidate.reset(joined.release());
	}
	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs))
	    return reject("could not materialize the edge curve");
	const auto validate_exact_locus = [surface, &trim, &edge, &edge_nurbs,
		&edge_domain, tolerance](const ON_Curve &curve,
		std::string *locus_failure) {
	    const ON_Interval curve_domain = curve.Domain();
	    double previous_directed_parameter = 0.0;
	    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
		    if (locus_failure)
			*locus_failure = "dense exact-locus validation was cancelled";
		    return false;
		}
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = curve.PointAt(
		    curve_domain.ParameterAt(fraction));
		const ON_3dPoint lift = uv.IsValid() ?
		    surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		const ON_3dPoint edge_point = edge.PointAt(
		    edge_domain.ParameterAt(trim.m_bRev3d ?
			1.0 - fraction : fraction));
		if (!lift.IsValid() || !edge_point.IsValid()) {
		    if (locus_failure)
			*locus_failure = "dense exact-locus evaluation was invalid";
		    return false;
		}
		double distance = lift.DistanceTo(edge_point);
		double directed_parameter = fraction;
		if (distance > tolerance) {
		    double closest_parameter = 0.0;
		    if (!ON_NurbsCurve_GetClosestPoint(&closest_parameter,
			    &edge_nurbs, lift)) {
			if (locus_failure)
			    *locus_failure = "closest edge-locus validation failed";
			return false;
		    }
		    distance = lift.DistanceTo(edge_nurbs.PointAt(closest_parameter));
		    directed_parameter = edge_nurbs.Domain().NormalizedParameterAt(
			closest_parameter);
		    if (trim.m_bRev3d)
			directed_parameter = 1.0 - directed_parameter;
		}
		if (distance > tolerance ||
			directed_parameter + kPeriodicParameterSnapFraction <
			previous_directed_parameter) {
		    if (locus_failure) {
			std::ostringstream reason;
			if (distance > tolerance)
			    reason << "candidate pcurve left the exact edge locus at "
				<< sample << '/' << kDenseValidationSegments
				<< " distance=" << distance << " tolerance="
				<< tolerance << " uv=" << uv.x << ':' << uv.y;
			else
			    reason << "candidate pcurve reversed along the edge locus at "
				<< sample << '/' << kDenseValidationSegments
				<< " parameter=" << directed_parameter
				<< " previous=" << previous_directed_parameter;
			*locus_failure = reason.str();
		    }
		    return false;
		}
		previous_directed_parameter = directed_parameter;
	    }
	    return true;
	};
	std::string locus_failure;
	if (!validate_exact_locus(*candidate, &locus_failure)) {
	    const ON_3dPoint exact_end = required_end ? *required_end :
		candidate_end;
	    const int fixed_direction = 1 - closed_direction;
	    if (!exact_end.IsValid() || fabs(required_start[fixed_direction] -
		    exact_end[fixed_direction]) > parameter_tolerance)
		return reject(locus_failure.c_str());
	    std::unique_ptr<ON_LineCurve> exact_iso(new ON_LineCurve(
		required_start, exact_end));
	    if (!exact_iso->ChangeDimension(2) ||
		    !exact_iso->SetDomain(trim.Domain().Min(), trim.Domain().Max()) ||
		    !exact_iso->IsValid() ||
		    !validate_exact_locus(*exact_iso, &locus_failure))
		return reject(locus_failure.empty() ?
		    "exact isoparametric pcurve was invalid" :
		    locus_failure.c_str());
	    candidate.reset(exact_iso.release());
	}
	if (candidate_out)
	    *candidate_out = candidate.release();
	return true;
    };

    for (int cut = 0; cut < source_loop.TrimCount(); ++cut) {
	const ON_BrepTrim *source_before = source_loop.Trim(cut);
	const ON_BrepTrim *source_after = source_loop.Trim(
	    (cut + 1) % source_loop.TrimCount());
	if (!source_before || !source_after || source_before->m_vi[1] < 0 ||
		source_before->m_vi[1] != source_after->m_vi[0] ||
		source_before->m_vi[1] >= brep->m_V.Count())
	    continue;
	const ON_3dPoint source_end = source_before->PointAtEnd();
	const ON_3dPoint source_start = source_after->PointAtStart();
	const double adjacent_start = source_start[closed_direction] + round(
	    (source_end[closed_direction] - source_start[closed_direction]) /
	    period) * period;
	/* On a doubly closed surface, the topology cut in closed_direction can
	 * also meet opposite but equivalent images in the nominal open
	 * direction.  Compare and seed that coordinate on one coherent periodic
	 * branch.  Treating 0 and one full period literally prevented an exact
	 * toroidal boundary chain from ever reaching the edge-driven proof below. */
	ON_3dPoint aligned_source_start = source_start;
	if (source_surface->IsClosed(open_direction)) {
	    const double open_period =
		source_surface->Domain(open_direction).Length();
	    if (open_period > ON_ZERO_TOLERANCE)
		aligned_source_start[open_direction] += round(
		    (source_end[open_direction] -
			aligned_source_start[open_direction]) / open_period) *
		    open_period;
	}
	if (!source_end.IsValid() || !source_start.IsValid() ||
		fabs(source_end[open_direction] -
		    aligned_source_start[open_direction]) >
		    parameter_tolerance ||
		fabs(source_end[closed_direction] - adjacent_start) >
		    parameter_tolerance) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": full-period boundary L" << loop_index << " cut "
		    << cut << " rejected by periodic endpoint continuity: end="
		    << source_end.x << ':' << source_end.y << " start="
		    << source_start.x << ':' << source_start.y << " aligned="
		    << aligned_source_start.x << ':' << aligned_source_start.y
		    << " tolerance=" << parameter_tolerance << std::endl;
	    continue;
	}
	const double native_image = closed_domain.Min() + round(
	    (source_start[closed_direction] - closed_domain.Min()) / period) *
	    period;

	double cut_tolerance = LocalUnits::tolerance;
	if (source_before->Edge())
	    cut_tolerance = std::max(cut_tolerance,
		source_before->Edge()->m_tolerance);
	if (source_after->Edge())
	    cut_tolerance = std::max(cut_tolerance,
		source_after->Edge()->m_tolerance);
	cut_tolerance = std::max(cut_tolerance,
	    brep->m_V[source_before->m_vi[1]].m_tolerance);
	const ON_3dPoint &cut_vertex =
	    brep->m_V[source_before->m_vi[1]].point;
	const ON_3dPoint source_end_lift = source_surface->PointAt(
	    source_end.x, source_end.y);
	const ON_3dPoint source_start_lift = source_surface->PointAt(
	    source_start.x, source_start.y);
	ON_3dPoint native_cut_uv = source_start;
	native_cut_uv[closed_direction] = native_image;
	const ON_3dPoint native_cut_lift = source_surface->PointAt(
	    native_cut_uv.x, native_cut_uv.y);
	/* A supplied pcurve endpoint can miss the numerical native seam while its
	 * STEP vertex lies on that seam within the edge's asserted tolerance.  This
	 * occurs after an exporter approximates a periodic spline boundary: a
	 * parameter-only test rejects the only topologically valid cut even though
	 * both periodic images lift to the same source vertex.  Admit that cut only
	 * with this independent 3-D proof.  The candidate pcurves below are still
	 * regenerated from the authoritative edges and densely validated before
	 * any topology is committed. */
	const bool numerical_native_seam =
	    fabs(source_start[closed_direction] - native_image) <=
		parameter_tolerance;
	const bool geometric_native_seam = native_cut_lift.IsValid() &&
	    native_cut_lift.DistanceTo(cut_vertex) <= cut_tolerance;
	if (!source_end_lift.IsValid() || !source_start_lift.IsValid() ||
		(!numerical_native_seam && !geometric_native_seam) ||
		source_end_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		source_start_lift.DistanceTo(cut_vertex) > cut_tolerance) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": full-period boundary L" << loop_index << " cut "
		    << cut << " rejected by native-seam topology proof: numerical="
		    << (numerical_native_seam ? "yes" : "no") << " geometric="
		    << (geometric_native_seam ? "yes" : "no") << " distances="
		    << source_end_lift.DistanceTo(cut_vertex) << '/'
		    << source_start_lift.DistanceTo(cut_vertex) << '/'
		    << native_cut_lift.DistanceTo(cut_vertex) << " tolerance="
		    << cut_tolerance << std::endl;
	    continue;
	}

	for (int winding = 0; winding < 2; ++winding) {
	    ON_BrepLoop &loop = brep->m_L[loop_index];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    std::vector<std::unique_ptr<ON_Curve> > regenerated_curves(
		loop.TrimCount());
	    std::vector<ON_BrepTrim *> regenerated_trims;
	    regenerated_trims.reserve(loop.TrimCount());
	    ON_3dPoint required_start = aligned_source_start;
	    required_start[closed_direction] = winding == 0 ?
		closed_domain.Min() : closed_domain.Max();
	    ON_3dPoint required_end = required_start;
	    required_end[closed_direction] = winding == 0 ?
		closed_domain.Max() : closed_domain.Min();
	    const ON_3dPoint start_lift = surface->PointAt(required_start.x,
		required_start.y);
	    const ON_3dPoint end_lift = surface->PointAt(required_end.x,
		required_end.y);
	    if (!start_lift.IsValid() || !end_lift.IsValid() ||
		    start_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		    end_lift.DistanceTo(cut_vertex) > cut_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected by native endpoint lift: "
			<< start_lift.DistanceTo(cut_vertex) << '/'
			<< end_lift.DistanceTo(cut_vertex) << " tolerance="
			<< cut_tolerance << " start=" << required_start.x << ':'
			<< required_start.y << " end=" << required_end.x << ':'
			<< required_end.y << std::endl;
		continue;
	    }

	    bool regenerated = true;
	    std::string failure;
	    int failed_trim_index = -1;
	    for (int offset = 0; regenerated && offset < loop.TrimCount();
		    ++offset) {
		ON_BrepTrim *trim = loop.Trim(
		    (cut + 1 + offset) % loop.TrimCount());
		if (wrapper) {
		    std::ostringstream detail;
		    detail << entity_type << " loop=L" << loop_index
			<< " cut=" << cut << " winding=" << winding;
		    if (trim)
			detail << " trim=T" << trim->m_trim_index;
		    wrapper->SetProgressDetail(
			"regenerating exact full-period boundary", entity_id,
			static_cast<uint64_t>(offset + 1),
			static_cast<uint64_t>(loop.TrimCount()), "trims",
			detail.str());
		}
		ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		ON_NurbsCurve edge_nurbs;
		double tolerance = LocalUnits::tolerance;
		if (edge)
		    tolerance = std::max(tolerance, edge->m_tolerance);
		if (trim)
		    tolerance = std::max(tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		if (trim && trim->m_vi[0] >= 0 &&
			trim->m_vi[0] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[0]].m_tolerance);
		if (trim && trim->m_vi[1] >= 0 &&
			trim->m_vi[1] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[1]].m_tolerance);
		const ON_3dPoint *last_endpoint =
		    offset + 1 == loop.TrimCount() ? &required_end : NULL;
		ON_Curve *regenerated_curve = NULL;
		const std::chrono::steady_clock::time_point trim_started =
		    std::chrono::steady_clock::now();
		std::string fast_path_failure;
		regenerated = trim && edge && translated_exact_pcurve(*trim, *edge,
		    tolerance, required_start, last_endpoint, &regenerated_curve,
		    &fast_path_failure);
		if (!regenerated) {
		    if (wrapper) {
			std::ostringstream detail;
			detail << entity_type << " loop=L" << loop_index
			    << " cut=" << cut << " winding=" << winding
			    << " trim=T" << (trim ? trim->m_trim_index : -1)
			    << " fallback=" << fast_path_failure;
			wrapper->SetProgressDetail(
			    "pulling exact full-period boundary from its edge",
			    entity_id, static_cast<uint64_t>(offset + 1),
			    static_cast<uint64_t>(loop.TrimCount()), "trims",
			    detail.str());
		    }
		    regenerated = trim && edge && edge->GetNurbForm(edge_nurbs) &&
			regenerate_trim_polyline(brep, *trim, surface,
			    edge_nurbs, tolerance, &failure, NULL, &required_start,
			    last_endpoint, true, wrapper, true, &regenerated_curve);
		}
		const long long trim_microseconds =
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - trim_started).count();
		if (wrapper->Verbose() && trim_microseconds >= 100000)
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index << "/T"
			<< (trim ? trim->m_trim_index : -1) << " cut/winding "
			<< cut << '/' << winding << " offset " << offset + 1 << '/'
			<< loop.TrimCount() << " elapsed="
			<< trim_microseconds / 1000.0 << "ms result="
			<< (regenerated ? "accepted" : "rejected") << std::endl;
		if (regenerated) {
		    regenerated_curves[offset].reset(regenerated_curve);
		    regenerated_trims.push_back(trim);
		    required_start = regenerated_curve->PointAtEnd();
		} else {
		    failed_trim_index = trim ? trim->m_trim_index : -1;
		}
	    }
	    if (!regenerated) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary regeneration L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected at T" << failed_trim_index << ": "
			<< (failure.empty() ? "no exact candidate" : failure)
			<< std::endl;
		continue;
	    }

	    bool exact_chain = true;
	    for (int offset = 0; exact_chain && offset < loop.TrimCount() - 1;
		    ++offset) {
		const ON_BrepTrim *previous = regenerated_trims[offset];
		const ON_BrepTrim *next = regenerated_trims[offset + 1];
		const double join_gap = regenerated_curves[offset] &&
		    regenerated_curves[offset + 1] ?
		    regenerated_curves[offset]->PointAtEnd().DistanceTo(
			regenerated_curves[offset + 1]->PointAtStart()) : DBL_MAX;
		exact_chain = previous && next && regenerated_curves[offset] &&
		    regenerated_curves[offset + 1] &&
		    previous->m_vi[1] == next->m_vi[0] &&
		    join_gap <= parameter_tolerance;
		if (!exact_chain && wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding << " join "
			<< offset << '/' << offset + 1 << " topology="
			<< (previous && next ? previous->m_vi[1] : -1) << '/'
			<< (previous && next ? next->m_vi[0] : -1) << " gap="
			<< join_gap << " tolerance=" << parameter_tolerance
			<< std::endl;
	    }
	    const ON_Curve *first_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.front().get();
	    const ON_Curve *last_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.back().get();
	    const bool boundary_endpoints_valid = first_curve && last_curve &&
		fabs(first_curve->PointAtStart()[closed_direction] -
		    (winding == 0 ? closed_domain.Min() :
			closed_domain.Max())) <= parameter_tolerance &&
		fabs(last_curve->PointAtEnd()[closed_direction] -
		    (winding == 0 ? closed_domain.Max() :
			closed_domain.Min())) <= parameter_tolerance &&
		fabs(first_curve->PointAtStart()[open_direction] -
		    last_curve->PointAtEnd()[open_direction]) <=
		    parameter_tolerance;

	    /* A segmented edge-driven boundary can retain a small interior UV
	     * residual even though every endpoint lift matches the same STEP
	     * topology vertex.  Reconcile only those interior joins on a temporary
	     * BREP.  The final-to-first full-period cut must remain open here so the
	     * face-band builder can materialize it as two explicit seam uses. */
	    if (!exact_chain && boundary_endpoints_valid) {
		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		bool candidate_valid = loop_index >= 0 &&
		    loop_index < candidate->m_L.Count();
		ON_BrepLoop *candidate_loop = candidate_valid ?
		    &candidate->m_L[loop_index] : NULL;
		std::vector<int> ordered_trim_indices;
		ordered_trim_indices.reserve(regenerated_trims.size());
		for (size_t offset = 0; candidate_valid &&
			offset < regenerated_trims.size(); ++offset) {
		    const int trim_index = regenerated_trims[offset] ?
			regenerated_trims[offset]->m_trim_index : -1;
		    ON_Curve *curve = regenerated_curves[offset] ?
			regenerated_curves[offset]->DuplicateCurve() : NULL;
		    const int c2_index = curve ?
			candidate->AddTrimCurve(curve) : -1;
		    if (trim_index < 0 || trim_index >= candidate->m_T.Count() ||
			    c2_index < 0 ||
			    !candidate->SetTrimCurve(candidate->m_T[trim_index],
				c2_index)) {
			if (curve && c2_index < 0)
			    delete curve;
			candidate_valid = false;
			break;
		    }
		    candidate->SetTrimIsoFlags(candidate->m_T[trim_index]);
		    ordered_trim_indices.push_back(trim_index);
		}
		if (candidate_valid) {
		    candidate_loop->m_ti.SetCount(0);
		    for (std::vector<int>::const_iterator trim_index =
			    ordered_trim_indices.begin();
			    trim_index != ordered_trim_indices.end(); ++trim_index)
			candidate_loop->m_ti.Append(*trim_index);
		    repair_adjacent_trim_endpoints(candidate.get(), wrapper,
			entity_id, entity_type, loop_index, false, true);
		    candidate_loop = &candidate->m_L[loop_index];
		    for (int lti = 0; candidate_valid &&
			    lti + 1 < candidate_loop->TrimCount(); ++lti) {
			const ON_BrepTrim *previous = candidate_loop->Trim(lti);
			const ON_BrepTrim *next = candidate_loop->Trim(lti + 1);
			candidate_valid = previous && next &&
			    previous->PointAtEnd().DistanceTo(
				next->PointAtStart()) <= ON_ZERO_TOLERANCE;
		    }
		    const ON_BrepTrim *candidate_first =
			candidate_loop->TrimCount() > 0 ?
			candidate_loop->Trim(0) : NULL;
		    const ON_BrepTrim *candidate_last =
			candidate_loop->TrimCount() > 0 ?
			candidate_loop->Trim(candidate_loop->TrimCount() - 1) :
			NULL;
		    candidate_valid = candidate_valid && candidate_first &&
			candidate_last &&
			fabs(candidate_first->PointAtStart()[closed_direction] -
			    (winding == 0 ? closed_domain.Min() :
				closed_domain.Max())) <= parameter_tolerance &&
			fabs(candidate_last->PointAtEnd()[closed_direction] -
			    (winding == 0 ? closed_domain.Max() :
				closed_domain.Min())) <= parameter_tolerance &&
			fabs(candidate_first->PointAtStart()[open_direction] -
			    candidate_last->PointAtEnd()[open_direction]) <=
			    parameter_tolerance;
		    for (int lti = 0; candidate_valid &&
			    lti < candidate_loop->TrimCount(); ++lti) {
			const ON_BrepTrim *trim = candidate_loop->Trim(lti);
			ON_wString trim_messages;
			ON_TextLog trim_log(trim_messages);
			candidate_valid = trim && trim->IsValid(&trim_log);
		    }
		}
		if (candidate_valid) {
		    *brep = *candidate;
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"closed residual joins in an exact full-period boundary chain");
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"unwrapped an exact full-period boundary from its 3-D STEP edge chain");
		    if (!numerical_native_seam && geometric_native_seam)
			wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			    "selected a periodic boundary cut from a tolerance-proven native seam vertex");
		    return true;
		}
	    }

	    if (!exact_chain || !boundary_endpoints_valid ||
		    fabs(first_curve->PointAtStart()[closed_direction] -
			(winding == 0 ? closed_domain.Min() :
			 closed_domain.Max())) > parameter_tolerance ||
		    fabs(last_curve->PointAtEnd()[closed_direction] -
			(winding == 0 ? closed_domain.Max() :
			 closed_domain.Min())) > parameter_tolerance ||
		    fabs(first_curve->PointAtStart()[open_direction] -
			last_curve->PointAtEnd()[open_direction]) > parameter_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected by final chain proof: exact="
			<< (exact_chain ? "yes" : "no") << " first="
			<< (first_curve ? first_curve->PointAtStart().x :
			    ON_UNSET_VALUE) << ':'
			<< (first_curve ? first_curve->PointAtStart().y :
			    ON_UNSET_VALUE) << " last="
			<< (last_curve ? last_curve->PointAtEnd().x :
			    ON_UNSET_VALUE) << ':'
			<< (last_curve ? last_curve->PointAtEnd().y :
			    ON_UNSET_VALUE) << " tolerance="
			<< parameter_tolerance << std::endl;
		continue;
	    }

	    /* Generate and prove every replacement before modifying the BREP.  The
	     * former transaction copied the complete solid for each candidate loop;
	     * a production CRM solid with roughly eleven thousand faces performed
	     * this copy more than a thousand times.  Curve ownership is committed
	     * only after every member of this one loop has validated. */
	    struct OriginalTrimCurveState {
		int c2_index;
		ON_Interval proxy_domain;
		ON_Interval trim_domain;
		ON_Surface::ISO iso;
		ON_BoundingBox pbox;
	    };
	    std::vector<OriginalTrimCurveState> original_states;
	    original_states.reserve(regenerated_trims.size());
	    for (std::vector<ON_BrepTrim *>::const_iterator trim =
		    regenerated_trims.begin(); trim != regenerated_trims.end(); ++trim) {
		OriginalTrimCurveState state = {
		    (*trim)->m_c2i, (*trim)->ProxyCurveDomain(), (*trim)->Domain(),
		    (*trim)->m_iso, (*trim)->m_pbox
		};
		original_states.push_back(state);
	    }
	    const int original_c2_count = brep->m_C2.Count();
	    std::vector<int> new_c2_indices;
	    new_c2_indices.reserve(regenerated_curves.size());
	    bool curves_added = true;
	    for (std::vector<std::unique_ptr<ON_Curve> >::iterator curve =
		    regenerated_curves.begin(); curve != regenerated_curves.end();
		    ++curve) {
		const int c2_index = curve->get() ?
		    brep->AddTrimCurve(curve->get()) : -1;
		if (c2_index < 0) {
		    curves_added = false;
		    break;
		}
		curve->release();
		new_c2_indices.push_back(c2_index);
	    }
	    const auto discard_added_curves = [brep, original_c2_count]() {
		for (int c2_index = original_c2_count;
			c2_index < brep->m_C2.Count(); ++c2_index)
		    delete brep->m_C2[c2_index];
		brep->m_C2.SetCount(original_c2_count);
	    };
	    if (!curves_added) {
		discard_added_curves();
		continue;
	    }
	    bool curves_installed = true;
	    size_t installed_count = 0;
	    for (; installed_count < regenerated_trims.size(); ++installed_count) {
		if (!brep->SetTrimCurve(*regenerated_trims[installed_count],
			new_c2_indices[installed_count])) {
		    curves_installed = false;
		    break;
		}
		brep->SetTrimIsoFlags(*regenerated_trims[installed_count]);
	    }
	    if (!curves_installed) {
		for (size_t restored = 0; restored < regenerated_trims.size();
			++restored) {
		    ON_BrepTrim &trim = *regenerated_trims[restored];
		    const OriginalTrimCurveState &state = original_states[restored];
		    brep->SetTrimCurve(trim, state.c2_index);
		    trim.SetProxyCurveDomain(state.proxy_domain);
		    trim.SetDomain(state.trim_domain);
		    trim.m_iso = state.iso;
		    trim.m_pbox = state.pbox;
		}
		discard_added_curves();
		continue;
	    }

	    /* The regenerated chain begins immediately after the proven native
	     * seam cut and ends immediately before it.  Preserve that traversal in
	     * the loop's cyclic storage as well: downstream pole-cut construction
	     * appends its two seam uses and singular trim at the last-to-first gap.
	     * Leaving the original rotation in place puts the one-period gap inside
	     * the array and makes an otherwise exact loop structurally invalid. */
	    std::vector<int> ordered_trim_indices;
	    ordered_trim_indices.reserve(loop.TrimCount());
	    for (int offset = 0; offset < loop.TrimCount(); ++offset)
		ordered_trim_indices.push_back(loop.m_ti[
		    (cut + 1 + offset) % loop.TrimCount()]);
	    loop.m_ti.SetCount(0);
	    for (std::vector<int>::const_iterator trim_index =
		    ordered_trim_indices.begin();
		    trim_index != ordered_trim_indices.end(); ++trim_index)
		loop.m_ti.Append(*trim_index);

	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact full-period boundary from its 3-D STEP edge chain");
	    if (!numerical_native_seam && geometric_native_seam) {
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "selected a periodic boundary cut from a tolerance-proven native seam vertex");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": accepted tolerance-proven native seam for L"
			<< loop_index << "/STEP" << source_loop.m_loop_user.i
			<< " cut/winding " << cut << '/' << winding
			<< " parameter-residual="
			<< fabs(source_start[closed_direction] - native_image)
			<< " topology-tolerance=" << cut_tolerance << std::endl;
	    }
	    return true;
	}
    }
    return false;
}


static bool
align_closed_surface_seam_from_trim_pair(ON_Brep *brep,
	const ON_BrepEdge &edge, const ON_BrepLoop &loop,
	const ON_Surface *surface, double tolerance,
	std::string *failure_reason = NULL)
{
    if (failure_reason)
	*failure_reason = "no closed surface direction admitted the seam pair";
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(
	const_cast<ON_Surface *>(surface));
    const ON_BrepFace *source_face = loop.Face();
	if (!brep || (!nurbs && !revolution) || !source_face ||
	    edge.m_ti.Count() != 2 ||
	    !(tolerance > 0.0))
	return false;

    const ON_BrepTrim *pair[2] = {
	brep->Trim(edge.m_ti[0]), brep->Trim(edge.m_ti[1])
    };
    if (!pair[0] || !pair[1] || pair[0]->m_li != loop.m_loop_index ||
	    pair[1]->m_li != loop.m_loop_index)
	return false;

    const auto fixed_iso_direction = [](ON_Surface::ISO iso) {
	if (iso == ON_Surface::W_iso || iso == ON_Surface::E_iso)
	    return 0;
	if (iso == ON_Surface::S_iso || iso == ON_Surface::N_iso)
	    return 1;
	return -1;
    };
    int preferred_direction = -1;
    {
	/* Invalid pcurves can report the wrong iso direction.  Infer the seam's
	 * fixed coordinate from immutable 3D edge samples pulled back onto the
	 * surface.  Whole-period unwrapping prevents a legitimate seam crossing
	 * from looking like a varying coordinate. */
	brlcad::PullbackContext context;
	double parameters[5][2];
	bool pulled = true;
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    const ON_3dPoint target = edge.PointAt(
		edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(surface->Domain(0).Mid(), surface->Domain(1).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    pulled = context.SurfaceClosestPoint(surface, target, uv, lift,
		distance, 0, std::max(1.0e-10, tolerance * 1.0e-6),
		tolerance) && distance <= tolerance;
	    if (!pulled)
		break;
	    for (int direction = 0; direction < 2; ++direction) {
		double parameter = uv[direction];
		if (sample > 0 && surface->IsClosed(direction)) {
		    const double period = surface->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			parameter += round((parameters[0][direction] - parameter) /
			    period) * period;
		}
		parameters[sample][direction] = parameter;
	    }
	}
	if (pulled) {
	    double normalized_range[2] = {DBL_MAX, DBL_MAX};
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		double minimum = parameters[0][direction];
		double maximum = parameters[0][direction];
		for (int sample = 1; sample < 5; ++sample) {
		    minimum = std::min(minimum, parameters[sample][direction]);
		    maximum = std::max(maximum, parameters[sample][direction]);
		}
		const double period = surface->Domain(direction).Length();
		if (period > ON_ZERO_TOLERANCE)
		    normalized_range[direction] = (maximum - minimum) / period;
	    }
	    const int exact_direction = normalized_range[0] <=
		normalized_range[1] ? 0 : 1;
	    const int varying_direction = 1 - exact_direction;
	    const double numerical_range = std::max(1.0e-8,
		1.0e-4 * normalized_range[varying_direction]);
	    if (normalized_range[exact_direction] <= numerical_range)
		preferred_direction = exact_direction;
	}
    }

    if (preferred_direction < 0) {
	preferred_direction = fixed_iso_direction(pair[0]->m_iso);
	const int second_iso_direction = fixed_iso_direction(pair[1]->m_iso);
	if (preferred_direction < 0)
	    preferred_direction = second_iso_direction;
	else if (second_iso_direction >= 0 &&
		second_iso_direction != preferred_direction)
	    return false;
    }

    /* Before iso flags have been recovered, a short seam can look nearly
     * constant in both coordinates on a doubly closed surface.  Its true
     * fixed direction is the coordinate with the smaller normalized span;
     * considering the other direction can rotate an unrelated surface seam
     * and invalidate every adjacent pcurve. */
    if (preferred_direction < 0) {
	double normalized_span[2] = {DBL_MAX, DBL_MAX};
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double period = surface->Domain(direction).Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		continue;
	    double maximum_span = 0.0;
	    for (int member = 0; member < 2; ++member) {
		const ON_Interval trim_domain = pair[member]->Domain();
		const int samples = std::min(256,
		    std::max(32, pair[member]->SpanCount() * 4));
		double minimum = DBL_MAX;
		double maximum = -DBL_MAX;
		for (int sample = 0; sample <= samples; ++sample) {
		    const ON_3dPoint uv = pair[member]->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		    minimum = std::min(minimum, uv[direction]);
		    maximum = std::max(maximum, uv[direction]);
		}
		maximum_span = std::max(maximum_span, maximum - minimum);
	    }
	    normalized_span[direction] = maximum_span / period;
	}
	if (normalized_span[0] < DBL_MAX || normalized_span[1] < DBL_MAX)
	    preferred_direction = normalized_span[0] <= normalized_span[1] ? 0 : 1;
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	if (preferred_direction >= 0 && direction != preferred_direction)
	    continue;
	if (revolution) {
	    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
	    if (direction != profile_direction || !revolution->m_curve ||
		    !revolution->m_curve->IsClosed())
		continue;
	}
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

	double minimum[2] = {DBL_MAX, DBL_MAX};
	double maximum[2] = {-DBL_MAX, -DBL_MAX};
	double center[2] = {0.0, 0.0};
	for (int member = 0; member < 2; ++member) {
	    const ON_Interval trim_domain = pair[member]->Domain();
	    const int samples = std::min(256,
		std::max(32, pair[member]->SpanCount() * 4));
	    for (int sample = 0; sample <= samples; ++sample) {
		const ON_3dPoint uv = pair[member]->PointAt(
		    trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		minimum[member] = std::min(minimum[member], uv[direction]);
		maximum[member] = std::max(maximum[member], uv[direction]);
		center[member] += uv[direction];
	    }
	    center[member] /= static_cast<double>(samples + 1);
	}
	const double branch_tolerance = 0.05 * period;
	if (maximum[0] - minimum[0] > branch_tolerance ||
		maximum[1] - minimum[1] > branch_tolerance ||
		fabs(fabs(center[0] - center[1]) - period) > branch_tolerance) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "trim pair did not describe opposite branches in direction "
		    << direction << " (spans " << maximum[0] - minimum[0]
		    << "/" << maximum[1] - minimum[1] << ", centers "
		    << center[0] << "/" << center[1] << ", period " << period
		    << ")";
		*failure_reason = reason.str();
	    }
	    continue;
	}

	double seam = std::max(center[0], center[1]);
	/* The supplied pcurves can be displaced by nearly the full asserted model
	 * uncertainty.  Using their average as a new surface seam then places the
	 * exact STEP edge just outside that boundary.  Refine interior edge samples
	 * from the supplied branch and use their constant exact surface parameter
	 * when the independent samples agree numerically. */
	double exact_parameters[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	bool exact_constant = true;
	const double numerical_tolerance = std::max(1.0e-10,
	    std::min(1.0e-7, tolerance * 1.0e-6));
	brlcad::PullbackContext pullback_context;
	const ON_Interval first_domain = pair[0]->Domain();
	const ON_Interval exact_edge_domain = edge.Domain();
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    ON_3dPoint uv = pair[0]->PointAt(first_domain.ParameterAt(fraction));
	    const double edge_fraction = pair[0]->m_bRev3d ?
		1.0 - fraction : fraction;
	    const ON_3dPoint target = edge.PointAt(
		exact_edge_domain.ParameterAt(edge_fraction));
	    double distance = DBL_MAX;
	    bool pulled = refine_surface_pullback_seeded(surface, target,
		numerical_tolerance, uv, &distance);
	    if (!pulled) {
		ON_2dPoint global_uv;
		ON_3dPoint global_lift;
		pulled = pullback_context.SurfaceClosestPoint(surface, target,
		    global_uv, global_lift, distance, 0, numerical_tolerance,
		    tolerance) && distance <= tolerance;
		if (pulled) {
		    uv.Set(global_uv.x, global_uv.y, 0.0);
		    double refined_distance = DBL_MAX;
		    if (refine_surface_pullback_seeded(surface, target,
			    numerical_tolerance, uv, &refined_distance))
			distance = refined_distance;
		}
	    }
	    if (!pulled) {
		exact_constant = false;
		break;
	    }
	    exact_parameters[sample] = uv[direction] +
		round((center[0] - uv[direction]) / period) * period;
	}
	if (exact_constant) {
	    double exact_minimum = exact_parameters[0];
	    double exact_maximum = exact_parameters[0];
	    double exact_sum = exact_parameters[0];
	    for (int sample = 1; sample < 5; ++sample) {
		exact_minimum = std::min(exact_minimum, exact_parameters[sample]);
		exact_maximum = std::max(exact_maximum, exact_parameters[sample]);
		exact_sum += exact_parameters[sample];
	    }
	    const double constant_tolerance = std::max(1.0e-9,
		1.0e-7 * period);
	    if (exact_maximum - exact_minimum <= constant_tolerance)
		seam = exact_sum / 5.0;
	    else if (failure_reason)
		*failure_reason = "exact edge pullbacks were not constant in the fixed direction";
	} else if (failure_reason) {
	    *failure_reason = "exact edge pullback could not locate the candidate surface seam";
	}
	while (seam < domain.Min()) seam += period;
	while (seam > domain.Max()) seam -= period;
	const double endpoint_guard = 1.0e-7 * std::max(1.0, period);
	if (seam <= domain.Min() + endpoint_guard ||
		seam >= domain.Max() - endpoint_guard) {
	    if (failure_reason)
		*failure_reason = "candidate seam was already on the surface domain boundary";
	    continue;
	}

	ON_NurbsSurface nurbs_candidate;
	ON_RevSurface revolution_candidate;
	const ON_Surface *candidate = NULL;
	if (nurbs) {
	    nurbs_candidate = *nurbs;
	    if (!nurbs_candidate.ChangeSurfaceSeam(direction, seam)) {
		if (failure_reason)
		    *failure_reason = "NURBS surface rejected the candidate seam parameter";
		continue;
	    }
	    candidate = &nurbs_candidate;
	} else {
	    revolution_candidate = *revolution;
	    if (!revolution_candidate.m_curve ||
		    !revolution_candidate.m_curve->ChangeClosedCurveSeam(seam))
		continue;
	    candidate = &revolution_candidate;
	}
	const ON_Interval candidate_domain = candidate->Domain(direction);
	ON_Xform shift(ON_Xform::IdentityTransformation);
	shift.m_xform[direction][3] = period;

	std::vector<int> trim_indices;
	std::vector<ON_Curve *> transformed_curves;
	bool all_valid = true;
	for (int fi = 0; fi < brep->m_F.Count() && all_valid; ++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    if (face.m_si != source_face->m_si)
		continue;
	    for (int fli = 0; fli < face.LoopCount() && all_valid; ++fli) {
		const ON_BrepLoop *affected_loop = face.Loop(fli);
		if (!affected_loop)
		    continue;
		for (int lti = 0; lti < affected_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = affected_loop->Trim(lti);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    ON_BoundingBox curve_box;
		    if (!trim || !curve || !curve->Transform(shift) ||
			    !curve->GetBoundingBox(curve_box)) {
			delete curve;
			all_valid = false;
			break;
		    }
		    const double curve_center = 0.5 *
			(curve_box.m_min[direction] + curve_box.m_max[direction]);
		    if (curve_center < candidate_domain.Min() - endpoint_guard ||
			    curve_center > candidate_domain.Max() + endpoint_guard) {
			const double wrap = round((candidate_domain.Mid() - curve_center) /
			    period) * period;
			if (fabs(wrap) > ON_ZERO_TOLERANCE) {
			    ON_Xform wrap_transform(ON_Xform::IdentityTransformation);
			    wrap_transform.m_xform[direction][3] = wrap;
			    if (!curve->Transform(wrap_transform)) {
				delete curve;
				all_valid = false;
				break;
			    }
			}
		    }
		    if (!curve->ChangeDimension(2) || !curve->IsValid()) {
			delete curve;
			all_valid = false;
			break;
		    }

		    const ON_Interval trim_domain = trim->Domain();
		    const int samples = std::min(256,
			std::max(32, trim->SpanCount() * 4));
		    for (int sample = 0; sample <= samples; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = trim->PointAt(parameter);
			const ON_3dPoint transformed_uv = curve->PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint transformed_lift = candidate->PointAt(
			    transformed_uv.x, transformed_uv.y);
			if (!original_lift.IsValid() || !transformed_lift.IsValid() ||
				original_lift.DistanceTo(transformed_lift) > tolerance) {
			    all_valid = false;
			    break;
			}
		    }
		    if (!all_valid) {
			delete curve;
			break;
		    }
		    trim_indices.push_back(trim->m_trim_index);
		    transformed_curves.push_back(curve);
		}
	    }
	}
	if (!all_valid) {
	    if (failure_reason)
		*failure_reason = "surface seam change did not preserve every affected pcurve lift";
	    for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
		    curve != transformed_curves.end(); ++curve)
		delete *curve;
	    continue;
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
	if (nurbs)
	    *nurbs = nurbs_candidate;
	else
	    *revolution = revolution_candidate;
	for (size_t i = 0; i < trim_indices.size(); ++i) {
	    if (!brep->SetTrimCurve(brep->m_T[trim_indices[i]], c2_indices[i]))
		return false;
	    brep->SetTrimIsoFlags(brep->m_T[trim_indices[i]]);
	}
	return true;
    }
    return false;
}


static bool
relocate_closed_surface_loop_seam(ON_Brep *brep, int loop_index,
	int direction, double tolerance, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, std::string *failure_reason,
	double requested_seam)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count() ||
	(direction != 0 && direction != 1) || !(tolerance > 0.0))
	return false;
    ON_BrepLoop &source_loop = brep->m_L[loop_index];
    ON_BrepFace *source_face = source_loop.Face();
    ON_Surface *surface = source_face ?
	const_cast<ON_Surface *>(source_face->SurfaceOf()) : NULL;
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
    ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    const int revolution_profile_direction = revolution ?
	(revolution->m_bTransposed ? 0 : 1) : -1;
    const int revolution_angle_direction = revolution ?
	(revolution->m_bTransposed ? 1 : 0) : -1;
    const bool supported_revolution_profile = revolution &&
	direction == revolution_profile_direction && revolution->m_curve &&
	revolution->m_curve->IsClosed();
    const bool supported_revolution_angle = revolution &&
	direction == revolution_angle_direction &&
	fabs(revolution->m_angle.Length() - ON_2PI) <= ON_ZERO_TOLERANCE;
    if ((!nurbs && !supported_revolution_profile &&
	    !supported_revolution_angle) || !surface ||
	    !surface->IsClosed(direction)) {
	if (failure_reason)
	    *failure_reason =
		"the loop surface does not expose a movable closed parameter seam";
	return false;
    }
    const int surface_index = source_face->m_si;
    const ON_Interval old_domain = surface->Domain(direction);
    const double period = old_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    double seam = requested_seam;
    if (std::isfinite(seam)) {
	while (seam < old_domain.Min()) seam += period;
	while (seam > old_domain.Max()) seam -= period;
    }

    /* Select the empty parameter interval from the authoritative 3-D STEP
     * edges, not from the supplied pcurves.  This repair is needed precisely
     * when an exporter chose a pcurve branch which extrapolates a
     * geometrically closed, non-periodic NURBS surface.  Such a pcurve can
     * make an occupied exact interval appear empty and move the surface seam
     * directly through the real boundary.  Native closest-point projections
     * provide cyclic phases only; the subsequent all-trim regeneration and
     * dense validation remain the transactional acceptance proof.
     *
     * A periodic face-band repair can instead request a seam which has been
     * independently proven to coincide with a supplied topology vertex.  In
     * that case every ordinary edge is required to stay on one side of the
     * requested seam by the same transactional projection below, while a
     * full-period singleton is rebuilt explicitly on the new native sides. */
    if (!std::isfinite(seam)) {
    std::vector<double> phases;
    /* Mark every cyclic phase interval traversed between adjacent exact-edge
     * samples.  Looking only at the sample points makes the largest sampling
     * gap indistinguishable from a genuinely boundary-free interval and can
     * place the new seam directly through the same edge. */
    std::vector<bool> occupied(kBoundaryParameterSearchSegments, false);
    brlcad::PullbackContext phase_pullback_context;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || !edge->EdgeCurveOf())
		    return false;
		/* A one-trim closed edge which winds the complete surface period
		 * necessarily intersects every possible seam.  It is reconstructed
		 * as an exact native-domain boundary after the seam move and must not
		 * hide the genuinely empty interval between ordinary open edges. */
		if (trim->m_vi[0] == trim->m_vi[1] &&
			edge->m_vi[0] == edge->m_vi[1])
		    continue;
		const int samples = std::min(256,
		    std::max(32, edge->SpanCount() * 4));
		const ON_Interval edge_domain = edge->Domain();
		const double projection_tolerance = std::max(tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		bool have_previous_phase = false;
		double previous_phase = 0.0;
		for (int sample = 0; sample <= samples; ++sample) {
		    if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) / samples;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trim->m_bRev3d ?
			    1.0 - fraction : fraction));
		    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint lift;
		    double distance = DBL_MAX;
		    if (!edge_point.IsValid() ||
			!phase_pullback_context.SurfaceClosestPoint(surface,
			    edge_point, uv, lift, distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				projection_tolerance * 0.1),
			    projection_tolerance) ||
			distance > projection_tolerance)
			return false;
		    double phase = fmod(uv[direction] - old_domain.Min(), period);
		    if (phase < 0.0)
			phase += period;
		    phases.push_back(phase);
		    if (!have_previous_phase) {
			const int bin = std::min(kBoundaryParameterSearchSegments - 1,
			    std::max(0, static_cast<int>(floor(phase / period *
				kBoundaryParameterSearchSegments))));
			occupied[bin] = true;
			have_previous_phase = true;
			previous_phase = phase;
			continue;
		    }
		    double delta = phase - previous_phase;
		    delta -= round(delta / period) * period;
		    const int interval_samples = std::max(1,
			static_cast<int>(ceil(fabs(delta) / period *
			    kBoundaryParameterSearchSegments * 2.0)));
		    for (int interval_sample = 0;
			    interval_sample <= interval_samples; ++interval_sample) {
			double occupied_phase = previous_phase + delta *
			    static_cast<double>(interval_sample) / interval_samples;
			occupied_phase = fmod(occupied_phase, period);
			if (occupied_phase < 0.0)
			    occupied_phase += period;
			const int bin = std::min(kBoundaryParameterSearchSegments - 1,
			    std::max(0, static_cast<int>(floor(occupied_phase /
				period * kBoundaryParameterSearchSegments))));
			occupied[bin] = true;
		    }
		    previous_phase = phase;
		}
	    }
	}
    }
    if (phases.size() < 2)
	return false;
    int largest_gap_bins = 0;
    int largest_gap_end = -1;
    int current_gap_bins = 0;
    for (int bin = 0; bin < 2 * kBoundaryParameterSearchSegments; ++bin) {
	if (!occupied[bin % kBoundaryParameterSearchSegments] &&
		current_gap_bins < kBoundaryParameterSearchSegments) {
	    ++current_gap_bins;
	    if (current_gap_bins > largest_gap_bins) {
		largest_gap_bins = current_gap_bins;
		largest_gap_end = bin;
	    }
	} else {
	    current_gap_bins = 0;
	}
    }
    const double largest_gap = period * largest_gap_bins /
	static_cast<double>(kBoundaryParameterSearchSegments);
    if (largest_gap < kMinimumSeamRelocationGapFraction * period) {
	if (failure_reason)
	    *failure_reason = "the affected boundary has no empty seam interval";
	return false;
    }
    const int largest_gap_start = largest_gap_end - largest_gap_bins + 1;
    double seam_phase = fmod((largest_gap_start + 0.5 * largest_gap_bins) *
	period / kBoundaryParameterSearchSegments, period);
    if (seam_phase < 0.0)
	seam_phase += period;
    seam = old_domain.Min() + seam_phase;
    }
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	period * 1.0e-10);
    if (seam <= old_domain.Min() + parameter_guard ||
	    seam >= old_domain.Max() - parameter_guard)
	return false;
    /* ON_RevSurface angle relocation explicitly changes the physical angle
     * represented by the unchanged m_t domain, so its existing pcurves need
     * the matching affine parameter shift.  ChangeSurfaceSeam() and a closed
     * profile curve's ChangeClosedCurveSeam() perform their own knot/domain
     * reparameterization; applying the same shift there would move exact
     * profile coordinates a second time. */
    const double seam_parameter_shift = supported_revolution_angle ?
	old_domain.Min() - seam : 0.0;

    ON_NurbsSurface nurbs_candidate;
    ON_RevSurface revolution_candidate;
    ON_Surface *candidate = NULL;
    if (nurbs) {
	nurbs_candidate = *nurbs;
	if (nurbs_candidate.ChangeSurfaceSeam(direction, seam) &&
		nurbs_candidate.IsValid())
	    candidate = &nurbs_candidate;
    } else if (supported_revolution_profile) {
	revolution_candidate = *revolution;
	if (revolution_candidate.m_curve &&
		revolution_candidate.m_curve->ChangeClosedCurveSeam(seam) &&
		revolution_candidate.IsValid())
	    candidate = &revolution_candidate;
    } else {
	revolution_candidate = *revolution;
	const double angle = revolution->m_angle.ParameterAt(
	    old_domain.NormalizedParameterAt(seam));
	revolution_candidate.m_angle.Set(angle, angle + ON_2PI);
	revolution_candidate.m_t.Set(old_domain.Min(), old_domain.Max());
	if (revolution_candidate.IsValid())
	    candidate = &revolution_candidate;
    }
    if (!candidate) {
	if (failure_reason)
	    *failure_reason = "openNURBS rejected the boundary-free seam";
	return false;
    }
    const ON_Interval candidate_domain = candidate->Domain(direction);
    struct Replacement {
	int trim_index;
	int original_c2_index;
	ON_Curve *seed;
	ON_Surface::ISO iso;
	ON_BrepTrim::TYPE type;
    };
    std::vector<Replacement> replacements;
    bool valid = true;
    for (int fi = 0; valid && fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; valid && fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; valid && lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		ON_Curve *source = trim ? trim->DuplicateCurve() : NULL;
		ON_BoundingBox box;
		if (!trim || !edge || !source || !source->GetBoundingBox(box)) {
		    delete source;
		    valid = false;
		    break;
		}
		ON_Curve *seed = NULL;
		std::string seed_failure_detail;
		const bool singleton_closed_boundary = loop->TrimCount() == 1 &&
		    trim->m_vi[0] == trim->m_vi[1] &&
		    edge->m_vi[0] == edge->m_vi[1];
		if (singleton_closed_boundary) {
		    /* A full-period singleton is rebuilt by the dedicated native-seam
		     * constructor after the candidate surface is installed.  Its open
		     * coordinate is unaffected by this seam move.  Translate its
		     * complete winding by the seam-coordinate delta first, then select
		     * the nearest whole-period image.  Merely wrapping the old pcurve
		     * leaves its topology vertex on the old revolution angle after an
		     * ON_RevSurface seam move. */
		    const double center = 0.5 *
			(box.m_min[direction] + box.m_max[direction]);
		    const double base_shift = seam_parameter_shift + round(
			(candidate_domain.Mid() -
			    (center + seam_parameter_shift)) / period) * period;
		    ON_Curve *curve = source->DuplicateCurve();
		    ON_Xform shift(ON_Xform::IdentityTransformation);
		    shift.m_xform[direction][3] = base_shift;
		    if (curve && curve->Transform(shift) &&
			    curve->ChangeDimension(2) && curve->IsValid())
			seed = curve;
		    else
			delete curve;
		} else {
		    /* ChangeSurfaceSeam is piecewise in parameter space: points below
		     * the new seam move by one period while points above it do not.  A
		     * uniform translation of a bad source pcurve therefore cannot be
		     * a reliable seed.  Project the immutable directed 3-D edge onto
		     * the candidate surface instead.  The seam was selected from an
		     * exact empty interval, so every ordinary edge must remain a
		     * continuous native-domain sequence; otherwise reject the whole
		     * transaction. */
		    const int samples = std::min(256,
			std::max(64, edge->SpanCount() * 4));
		    const ON_Interval edge_domain = edge->Domain();
		    const ON_Interval trim_domain = trim->Domain();
		    const double projection_tolerance = std::max(tolerance,
			std::max(edge->m_tolerance,
			    std::max(trim->m_tolerance[0],
				trim->m_tolerance[1])));
		    ON_3dPointArray points;
		    ON_SimpleArray<double> parameters;
		    points.Reserve(samples + 1);
		    parameters.Reserve(samples + 1);
		    brlcad::PullbackContext seed_pullback_context;
		    double previous_parameter = 0.0;
		    bool continuous = edge_domain.IsIncreasing() &&
			trim_domain.IsIncreasing();
		    for (int sample = 0; continuous && sample <= samples; ++sample) {
			if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled()) {
			    continuous = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) / samples;
			const ON_3dPoint edge_point = edge->PointAt(
			    edge_domain.ParameterAt(trim->m_bRev3d ?
				1.0 - fraction : fraction));
			ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
			ON_3dPoint pulled_lift;
			double pulled_distance = DBL_MAX;
			continuous = edge_point.IsValid() &&
			    seed_pullback_context.SurfaceClosestPoint(candidate,
				edge_point, pulled_uv, pulled_lift,
				pulled_distance, 0,
				std::max(ON_ZERO_TOLERANCE,
				    projection_tolerance * 0.1),
				projection_tolerance) &&
			    pulled_distance <= projection_tolerance;
			if (!continuous) {
			    std::ostringstream detail;
			    detail << "STEP edge #" << edge->m_edge_user.i
				<< " projection failed at sample " << sample << '/'
				<< samples << " (distance " << pulled_distance
				<< ", tolerance " << projection_tolerance << ')';
			    seed_failure_detail = detail.str();
			    break;
			}
			double candidate_parameter = pulled_uv[direction];
			if (sample > 0)
			    candidate_parameter += round((previous_parameter -
				candidate_parameter) / period) * period;
			const double guard = std::max(ON_ZERO_TOLERANCE *
			    kNumericalToleranceScale, period * 1.0e-10);
			if (candidate_parameter < candidate_domain.Min() - guard ||
				candidate_parameter > candidate_domain.Max() + guard) {
			    std::ostringstream detail;
			    detail << "STEP edge #" << edge->m_edge_user.i
				<< " crossed the candidate native seam at sample "
				<< sample << '/' << samples << " (parameter "
				<< candidate_parameter << ", domain "
				<< candidate_domain.Min() << ':'
				<< candidate_domain.Max() << ')';
			    seed_failure_detail = detail.str();
			    continuous = false;
			    break;
			}
			pulled_uv[direction] = std::max(candidate_domain.Min(),
			    std::min(candidate_domain.Max(), candidate_parameter));
			previous_parameter = pulled_uv[direction];
			points.Append(ON_3dPoint(pulled_uv.x, pulled_uv.y, 0.0));
			parameters.Append(trim_domain.ParameterAt(fraction));
		    }
		    if (continuous && points.Count() >= 2) {
			ON_PolylineCurve *curve = new ON_PolylineCurve(points,
			    parameters);
			if (curve && curve->ChangeDimension(2) && curve->IsValid())
			    seed = curve;
			else
			    delete curve;
		    }
		}
		if (!seed) {
		    delete source;
		    valid = false;
		    if (failure_reason)
			*failure_reason = "an exact edge could not be seeded continuously inside the relocated surface domain" +
			    (seed_failure_detail.empty() ? std::string() :
				std::string(": ") + seed_failure_detail);
		    break;
		}
		/* SetTrimCurve() changes only the trim's curve index; it does not
		 * mutate or remove the existing m_C2 entry.  Retain that index for
		 * transactional rollback instead of keeping a complete duplicate of
		 * every source pcurve and adding another duplicate on failure.  On
		 * giant multi-loop faces the former rollback path retained thousands
		 * of unreferenced curves until final BREP compaction. */
		const int original_c2_index = trim->m_c2i;
		delete source;
		replacements.push_back({trim->m_trim_index, original_c2_index, seed,
		    trim->m_iso, trim->m_type});
	    }
	}
    }
    if (!valid) {
	for (std::vector<Replacement>::iterator replacement = replacements.begin();
		replacement != replacements.end(); ++replacement)
	{
	    delete replacement->seed;
	}
	return false;
    }

    std::vector<int> curve_indices(replacements.size(), -1);
    for (size_t index = 0; index < replacements.size(); ++index) {
	curve_indices[index] = brep->AddTrimCurve(replacements[index].seed);
	if (curve_indices[index] < 0)
	    return false;
	replacements[index].seed = NULL;
    }
    ON_NurbsSurface original_nurbs;
    ON_RevSurface original_revolution;
    if (nurbs) {
	original_nurbs = *nurbs;
	*nurbs = nurbs_candidate;
    } else {
	original_revolution = *revolution;
	*revolution = revolution_candidate;
    }
    for (size_t index = 0; index < replacements.size(); ++index) {
	if (!brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		curve_indices[index]))
	    return false;
	brep->SetTrimIsoFlags(brep->m_T[replacements[index].trim_index]);
    }
    bool regenerated = true;
    std::string regeneration_failure;
    int failed_trim_index = -1;
    int failed_step_edge = 0;
    for (size_t index = 0; regenerated && index < replacements.size(); ++index) {
	ON_BrepTrim &trim = brep->m_T[replacements[index].trim_index];
	ON_BrepEdge *edge = trim.Edge();
	ON_NurbsCurve edge_nurbs;
	const double trim_tolerance = edge ? std::max(tolerance,
	    std::max(edge->m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]))) : tolerance;
	ON_BrepLoop *owner_loop = trim.Loop();
	const bool singleton_closed_boundary = edge && owner_loop &&
	    owner_loop->TrimCount() == 1 &&
	    trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	regenerated = false;
	if (singleton_closed_boundary) {
	    const ON_3dPoint seed = trim.PointAt(trim.Domain().Mid());
	    for (int closed_direction = 0; !regenerated &&
		    closed_direction < 2; ++closed_direction) {
		if (!surface->IsClosed(closed_direction) || !seed.IsValid())
		    continue;
		regenerated = regenerate_native_seam_periodic_boundary(brep,
		    *owner_loop, surface, closed_direction, wrapper, entity_id,
		    entity_type, seed[1 - closed_direction], false);
	    }
	    if (!regenerated) {
		/* The seam was intentionally aligned to a vertex of the sibling
		 * boundary.  A closed singleton can have a different arbitrary STEP
		 * topology vertex, so it cannot yet be represented as one native-side
		 * pcurve.  Retain its exact translated full-period winding only when
		 * every directed sample still lifts to the immutable edge.  The
		 * periodic-band pass immediately restarts and materializes the native
		 * seam crossing as an explicit shared edge split; this temporary
		 * state is never accepted as a final OpenNURBS loop. */
		const ON_Curve *temporary = trim.TrimCurveOf();
		const ON_Curve *original = replacements[index].original_c2_index >= 0 &&
		    replacements[index].original_c2_index < brep->m_C2.Count() ?
		    brep->m_C2[replacements[index].original_c2_index] : NULL;
		const ON_Surface *original_surface = nurbs ?
		    static_cast<const ON_Surface *>(&original_nurbs) :
		    static_cast<const ON_Surface *>(&original_revolution);
		const ON_Interval temporary_domain = temporary ?
		    temporary->Domain() : ON_Interval::EmptyInterval;
		const ON_Interval original_domain = original ?
		    original->Domain() : ON_Interval::EmptyInterval;
		int winding_direction = -1;
		if (temporary && original && temporary_domain.IsIncreasing() &&
			original_domain.IsIncreasing()) {
		    const ON_3dPoint start = temporary->PointAtStart();
		    const ON_3dPoint end = temporary->PointAtEnd();
		    for (int candidate_direction = 0;
			    candidate_direction < 2; ++candidate_direction) {
			if (!surface->IsClosed(candidate_direction))
			    continue;
			const double candidate_period =
			    surface->Domain(candidate_direction).Length();
			const double parameter_tolerance = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    kPeriodicParameterSnapFraction *
				std::max(1.0, candidate_period));
			if (candidate_period > ON_ZERO_TOLERANCE &&
				fabs(fabs(end[candidate_direction] -
				    start[candidate_direction]) -
				    candidate_period) <= parameter_tolerance &&
				fabs(end[1 - candidate_direction] -
				    start[1 - candidate_direction]) <=
				    parameter_tolerance) {
			    winding_direction = candidate_direction;
			    break;
			}
		    }
		}
		/* This is an affine parameterization change on one immutable
		 * surface.  Prove the retained pcurve by comparing its new-surface
		 * lift to the original pcurve's old-surface lift at dense identical
		 * parameters.  Comparing both to the edge at the same normalized
		 * parameter is incorrect here: fitted pullbacks preserve the edge
		 * locus but are not required to share its parameterization. */
		const double exact_reparameterization_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    trim_tolerance * 1.0e-8);
		bool exact_temporary = winding_direction >= 0;
		double maximum_temporary_distance = 0.0;
		int maximum_temporary_sample = -1;
		for (int sample = 0; exact_temporary &&
			sample <= kDenseValidationSegments; ++sample) {
		    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
			exact_temporary = false;
			break;
		    }
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint original_uv = original->PointAt(
			original_domain.ParameterAt(fraction));
		    const ON_3dPoint uv = temporary->PointAt(
			temporary_domain.ParameterAt(fraction));
		    const ON_3dPoint original_lift = original_surface->PointAt(
			original_uv.x, original_uv.y);
		    const ON_3dPoint relocated_lift = surface->PointAt(uv.x, uv.y);
		    const double distance =
			original_lift.IsValid() && relocated_lift.IsValid() ?
			original_lift.DistanceTo(relocated_lift) : DBL_MAX;
		    if (distance > maximum_temporary_distance) {
			maximum_temporary_distance = distance;
			maximum_temporary_sample = sample;
		    }
		    exact_temporary =
			distance <= exact_reparameterization_tolerance;
		}
		regenerated = exact_temporary;
		if (!regenerated) {
		    regeneration_failure =
			"could not retain an exact full-period singleton pending its native seam split";
		    if (wrapper->Verbose()) {
			const ON_3dPoint temporary_start = temporary ?
			    temporary->PointAtStart() : ON_3dPoint::UnsetPoint;
			const ON_3dPoint temporary_end = temporary ?
			    temporary->PointAtEnd() : ON_3dPoint::UnsetPoint;
			std::cerr << entity_type << " #" << entity_id
			    << ": relocated full-period singleton T"
			    << trim.m_trim_index << " rejected winding-direction="
			    << winding_direction << " uv=" << temporary_start.x
			    << ':' << temporary_start.y << "->"
			    << temporary_end.x << ':' << temporary_end.y
			    << " maximum-distance=" << maximum_temporary_distance
			    << " sample=" << maximum_temporary_sample
			    << " tolerance=" << exact_reparameterization_tolerance
			    << std::endl;
		    }
		}
	    }
	} else {
	    /* The temporary curve was projected from the exact directed edge onto
	     * the candidate surface and already preserves every loop join.  A
	     * seam move in one parameter direction must not let the generic
	     * pullback choose a different lift-equivalent image in the other
	     * closed direction: doing so tears an otherwise valid torus band by
	     * exactly one period.  Pin both projected endpoint images while the
	     * interior is regenerated from the immutable 3-D edge.  The helper
	     * still densely validates the complete curve and both endpoint lifts,
	     * so these are continuity constraints rather than accepted geometry. */
	    const ON_3dPoint required_start = trim.PointAtStart();
	    const ON_3dPoint required_end = trim.PointAtEnd();
	    regenerated = edge && edge->GetNurbForm(edge_nurbs) &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    trim_tolerance, &regeneration_failure, NULL,
		    &required_start, &required_end, true, wrapper, true);
	}
	if (!regenerated) {
	    failed_trim_index = trim.m_trim_index;
	    failed_step_edge = edge ? edge->m_edge_user.i : 0;
	}
    }
    if (!regenerated) {
	/* Restore the pre-relocation surface and pcurves.  Newly added candidate
	 * curves remain unreferenced and are compacted with the BREP later. */
	if (nurbs)
	    *nurbs = original_nurbs;
	else
	    *revolution = original_revolution;
	if (failure_reason) {
	    std::ostringstream reason;
	    reason << "exact-edge regeneration on the relocated surface failed";
	    if (failed_trim_index >= 0)
		reason << " for trim T" << failed_trim_index;
	    if (failed_step_edge > 0)
		reason << "/STEP edge #" << failed_step_edge;
	    reason << ": " << regeneration_failure;
	    *failure_reason = reason.str();
	}
	for (size_t index = 0; index < replacements.size(); ++index) {
	    const int original_index = replacements[index].original_c2_index;
	    if (original_index >= 0 && original_index < brep->m_C2.Count()) {
		brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		    original_index);
		brep->m_T[replacements[index].trim_index].m_iso = replacements[index].iso;
		brep->m_T[replacements[index].trim_index].m_type = replacements[index].type;
	    }
	}
	return false;
    }
    return true;
}


/* Report whether an ordinary open boundary crosses the native seam of a
 * geometrically closed surface.  Parameter samples are obtained from the
 * immutable 3-D edge and unwrapped in traversal order, so a bad supplied
 * pcurve cannot hide the crossing.  This is only a trigger for the
 * transactional whole-surface seam relocation above; it never changes
 * topology or accepts geometry by itself. */
static bool
exact_open_trim_crosses_native_seam(const ON_BrepTrim &trim,
	const ON_Surface *surface, int direction, double tolerance,
	brlcad::PullbackContext &pullback_context)
{
    const ON_BrepEdge *edge = trim.Edge();
    if (!surface || !edge || direction < 0 || direction > 1 ||
	    !surface->IsClosed(direction) ||
	    trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
	    trim.m_vi[0] == trim.m_vi[1] ||
	    edge->m_vi[0] == edge->m_vi[1])
	return false;
    /* A trim which is an OpenNURBS seam in one surface direction can still
     * cross the native seam in the other direction (for example, a short
     * meridian edge crossing a torus profile cut).  Exclude only the
     * coordinate held fixed by the trim's established ISO classification. */
    int fixed_direction = -1;
    if (trim.m_iso == ON_Surface::W_iso ||
	    trim.m_iso == ON_Surface::E_iso)
	fixed_direction = 0;
    else if (trim.m_iso == ON_Surface::S_iso ||
	    trim.m_iso == ON_Surface::N_iso)
	fixed_direction = 1;
    if (trim.m_type == ON_BrepTrim::seam &&
	    fixed_direction == direction)
	return false;
    const ON_Interval surface_domain = surface->Domain(direction);
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge->Domain();
    if (!surface_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing())
	return false;
    const double period = surface_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    const int samples = std::min(256, std::max(64, edge->SpanCount() * 4));
    const double projection_tolerance = std::max(tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
    const double seam_guard = std::max(ON_ZERO_TOLERANCE *
	kNumericalToleranceScale, period * 1.0e-10);
    const bool surface_closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    const ON_Interval surface_domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    bool have_previous = false;
    double previous = 0.0;
    for (int sample = 0; sample <= samples; ++sample) {
	if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) / samples;
	const ON_3dPoint edge_point = edge->PointAt(edge_domain.ParameterAt(
	    trim.m_bRev3d ? 1.0 - fraction : fraction));
	const ON_3dPoint trim_uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	const ON_2dPoint seed(trim_uv.x, trim_uv.y);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() || !seed.IsValid())
	    return false;
	/* The supplied pcurve is only a numerical seed: acceptance is still based
	 * on projecting the immutable 3-D STEP edge and validating its lift.  A
	 * local seeded solve keeps successive samples on the pcurve's coherent
	 * periodic branch and avoids rebuilding the same surface span index for
	 * every trim.  If that local proof fails, retain the historical global
	 * closest-point search so a bad supplied pcurve cannot hide a crossing. */
	const double solver_tolerance = std::max(ON_ZERO_TOLERANCE,
	    projection_tolerance * 0.1);
	bool projected = pullback_context.SurfaceClosestPointFromSeed(surface,
	    edge_point, seed, uv, lift, distance, projection_tolerance,
	    surface_closed, surface_domains, solver_tolerance) &&
	    distance <= projection_tolerance;
	if (!projected) {
	    uv = ON_2dPoint::UnsetPoint;
	    distance = DBL_MAX;
	    projected = pullback_context.SurfaceClosestPoint(surface, edge_point,
		uv, lift, distance, 0, solver_tolerance,
		projection_tolerance) && distance <= projection_tolerance;
	}
	if (!projected)
	    return false;
	double current = uv[direction];
	if (have_previous)
	    current += round((previous - current) / period) * period;
	if (have_previous) {
	    const double minimum = std::min(previous, current);
	    const double maximum = std::max(previous, current);
	    const double first_seam = surface_domain.Min() +
		ceil((minimum - surface_domain.Min()) / period) * period;
	    if (first_seam > minimum + seam_guard &&
		    first_seam < maximum - seam_guard)
		return true;
	    /* An exact interior sample on the seam is also a crossing even when
	     * floating-point unwrapping places both neighboring samples on the
	     * same side.  Endpoints are harmless: adjacent trims may legitimately
	     * meet on the native boundary. */
	    const double nearest_seam = surface_domain.Min() +
		round((current - surface_domain.Min()) / period) * period;
	    if (sample < samples &&
		    fabs(current - previous) > seam_guard &&
		    fabs(current - nearest_seam) <= seam_guard)
		return true;
	}
	previous = current;
	have_previous = true;
    }
    return false;
}


static size_t
repair_closed_surface_seam_crossings(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;
    size_t repaired = 0;
    std::set<int> repaired_surfaces;
    std::set<std::pair<int, int> > attempted_surface_directions;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop &loop = brep->m_L[li];
	ON_BrepFace *face = loop.Face();
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
		bool crossing = exact_open_trim_crosses_native_seam(*current,
		    surface, direction, LocalUnits::tolerance, pullback_context);
		    if (!crossing && current->m_type != ON_BrepTrim::seam &&
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
			crossing = !end_lift.IsValid() || !start_lift.IsValid() ||
			    end_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
			    start_lift.DistanceTo(vertex) > LocalUnits::tolerance;
		    }
		if (!crossing)
		    continue;
		attempted_surface_directions.insert(attempt);
		std::string failure;
		if (relocate_closed_surface_loop_seam(brep, li, direction,
			LocalUnits::tolerance, wrapper, entity_id, entity_type,
			&failure)) {
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


static void
repair_seam_pair_from_exact_edge(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> *additional_repaired_loops = NULL,
	std::vector<int> *aligned_surface_loops = NULL,
	bool allow_surface_alignment = false)
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
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;

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
	    if (!previous || !next || previous->m_vi[1] < 0 ||
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


static void
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
static double
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


static bool
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


static void
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


static void
repair_bounded_seam_isos(ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, bool allow_surface_alignment,
	std::vector<int> *aligned_surface_loops_out = NULL)
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


static void
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


static double
verified_regeneration_tolerance(ON_BrepTrim &trim, ON_BrepEdge &edge,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, const ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type);

static double
verified_source_pcurve_tolerance(ON_BrepTrim &trim, ON_BrepEdge &edge,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, const ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type);


static void
repair_invalid_open_pcurves(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool repair_periodic_join_discontinuities = false)
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
		!invalid_curve_locus)
	    continue;
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
			!invalid_curve_locus)
		    continue;
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


static void
repair_missing_singular_trims(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int repair_budget = brep->m_T.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
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
	}
	if (!changed)
	    break;
    }
}


static bool
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


static size_t
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
static ON_Curve *
translated_periodic_trim_for_join(const ON_Surface *surface,
    const ON_BrepTrim &trim, const ON_3dPoint &join, bool move_start,
    std::string *failure, double bounded_lift_tolerance = -1.0,
    double *measured_bounded_mismatch = NULL)
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


static double
verified_source_pcurve_tolerance(ON_BrepTrim &trim,
	ON_BrepEdge &edge, const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance,
	const ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    const ON_Curve *source_curve = trim.TrimCurveOf();
    if (!source_curve || !surface || !wrapper ||
	    wrapper->ImportOptions().exact ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    !(tolerance > 0.0))
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
	 * parameterized curves can describe the same locus.  Do the expensive
	 * closest-locus query only when this sample could increase the measured
	 * maximum.  A failed solve cannot prove the source association and hence
	 * cannot authorize a tolerance adjustment. */
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
    if (!(adjusted <= limit))
	return tolerance;

    edge.m_tolerance = std::max(edge.m_tolerance, adjusted);
    trim.m_tolerance[0] = std::max(trim.m_tolerance[0], adjusted);
    trim.m_tolerance[1] = std::max(trim.m_tolerance[1], adjusted);
    if (wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id << ": trim "
	    << trim.m_trim_index << "/STEP edge " << edge.m_edge_user.i
	    << " existing pcurve/edge mismatch=" << measured
	    << " declared/existing tolerance=" << tolerance
	    << " adjusted tolerance=" << adjusted << " bounded limit="
	    << limit << std::endl;
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	entity_id, entity_type, "trim_pcurve",
	"existing pcurve and 3-D edge exceeded the declared tolerance after "
	"exact regeneration failed; "
	"used a densely measured OpenNURBS tolerance");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"adjusted one trim tolerance to densely measured existing pcurve geometry");
    return adjusted;
}


static double
verified_regeneration_tolerance(ON_BrepTrim &trim,
	ON_BrepEdge &edge, const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance,
	const ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!surface || !wrapper || wrapper->ImportOptions().exact ||
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


static size_t
repair_exact_periodic_loop_branches(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
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


static size_t
regenerate_periodic_loop_chains(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, int only_loop = -1,
	bool record_repair = true)
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
		    wrapper, has_paired_edge_use);
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


static void
repair_adjacent_trim_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, int only_loop,
	bool record_changes, bool skip_closing_join)
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
    std::set<int> measured_singular_join_trims;
    std::set<int> translated_trim_indices;
    std::map<int, std::set<std::vector<double> > > seen_loop_states;

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

    std::vector<bool> dirty(static_cast<size_t>(brep->m_L.Count()),
	only_loop < 0);
    if (only_loop >= 0)
	dirty[static_cast<size_t>(only_loop)] = true;

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
	    if (!surface || loop.TrimCount() < 2)
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
			    measured_singular_join_trims.insert(
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
		    const double bounded_lift_tolerance =
			!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe ?
			join_tolerance *
			    kPeriodicClosureToleranceMaximumScale : -1.0;
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
		    const double bounded_lift_tolerance =
			!wrapper->ImportOptions().exact &&
			wrapper->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe ?
			join_tolerance : -1.0;
		    ON_Curve *translated = translated_trim_indices.find(
			next->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *next,
			    previous_uv, true, &translation_failure,
			    bounded_lift_tolerance) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*next, c2_index)) {
			    brep->SetTrimIsoFlags(*next);
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
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic ordinary join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
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
		    const ON_3dPoint seam_join = movable == previous ? next_uv :
			previous_uv;
		    std::string translation_failure;
		    ON_Curve *translated = translated_trim_indices.find(
			movable->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *movable,
			    seam_join, movable == next, &translation_failure) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
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
		    ON_BrepEdge *movable_edge = movable->Edge();
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
		     previous_candidate->IsValid());
		const bool next_constructed = !change_next ||
		    (next_candidate &&
		     (next->m_type == ON_BrepTrim::singular ||
			next->m_type == ON_BrepTrim::seam ||
			next_candidate->SetStartPoint(common)) &&
		     next_candidate->ChangeDimension(2) && next_candidate->IsValid());
		if (!change_previous && !change_next) {
		    delete previous_candidate;
		    delete next_candidate;
		    continue;
		}

		const auto validates = [brep, surface, join_tolerance, wrapper](
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
		    if (original.m_type == ON_BrepTrim::seam ||
			    original.m_type == ON_BrepTrim::singular) {
			const ON_Surface::ISO derived = surface->IsIsoparametric(
			    candidate, &candidate_domain);
			if (derived != original.m_iso) {
			    if (failure)
				*failure = original.m_type == ON_BrepTrim::seam ?
				    "seam iso classification changed" :
				    "singular boundary iso classification changed";
			    return false;
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
			     previous_alternative->IsValid());
			const bool alternative_next_constructed =
			    !alternative_change_next ||
			    (next_alternative &&
			     (next->m_type == ON_BrepTrim::seam ||
				next_alternative->SetStartPoint(
				    alternative_common)) &&
			     next_alternative->ChangeDimension(2) &&
			     next_alternative->IsValid());
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
			ON_BrepTrim *invalid_trim = NULL;
			if (!previous_valid && previous->m_type != ON_BrepTrim::seam &&
				previous_failure.compare(0, 13, "edge distance") == 0)
			    invalid_trim = previous;
			else if (!next_valid && next->m_type != ON_BrepTrim::seam &&
				next_failure.compare(0, 13, "edge distance") == 0)
			    invalid_trim = next;
			bool regenerated = false;
			std::string regeneration_failure;
			if (invalid_trim && attempted_edge_regeneration.insert(
				invalid_trim->m_trim_index).second) {
			    const ON_BrepEdge *invalid_edge = invalid_trim->Edge();
			    ON_NurbsCurve edge_nurbs;
			    if (invalid_edge && invalid_edge->GetNurbForm(edge_nurbs))
				regenerated = regenerate_trim_polyline(brep, *invalid_trim,
				    surface, edge_nurbs, LocalUnits::tolerance,
				    &regeneration_failure, NULL, NULL, NULL, false,
				    wrapper);
			}
			if (regenerated) {
			    record_repair("trim_pcurve",
				"regenerated an invalid pcurve from the exact edge");
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " regenerated trim "
				    << invalid_trim->m_trim_index
				    << " from its exact edge before endpoint repair" << std::endl;
			    delete previous_candidate;
			    delete next_candidate;
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id << ": loop "
				<< li << " endpoint candidate "
				<< previous->m_trim_index << '/' << next->m_trim_index
				<< " rejected: previous="
				<< (previous_valid ? "valid" : previous_failure)
				<< ", next="
				<< (next_valid ? "valid" : next_failure)
				<< (regeneration_failure.empty() ? "" :
				    ", exact-edge regeneration=" + regeneration_failure)
				<< std::endl;
			delete previous_candidate;
			delete next_candidate;
			continue;
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
			<< join_tolerance << std::endl;
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


static size_t
repair_paired_seam_loop_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_pairs = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 4)
	    continue;

	std::map<int, std::vector<int> > seam_positions;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::seam && trim->m_ei >= 0)
		seam_positions[trim->m_ei].push_back(lti);
	}
	for (std::map<int, std::vector<int> >::const_iterator pair =
		seam_positions.begin(); pair != seam_positions.end(); ++pair) {
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
		}
		if (!complete)
		    break;

		double tolerance = std::max(LocalUnits::tolerance,
		    edge.m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
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


static void
repair_zero_length_boundary_edges(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const ON_BoundingBox item_bounds = brep->BoundingBox();
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
	    const ON_Interval domain = edge.Domain();
	    const ON_3dPoint vertex = brep->m_V[edge.m_vi[0]].point;
	    double maximum_radius = 0.0;
	    for (int sample = 0; sample <= 64; ++sample) {
		const ON_3dPoint point = edge.PointAt(domain.ParameterAt(
		    static_cast<double>(sample) / 64.0));
		const double radius = point.IsValid() ? point.DistanceTo(vertex) : DBL_MAX;
		maximum_radius = std::max(maximum_radius, radius);
	    }
	    /* A source-asserted zero-length topology edge can carry a short open
	     * spline spur whose endpoint and surface discrepancies have already
	     * established a larger, densely measured local edge tolerance.  Once
	     * the complete spur is proven inside that ball, removing its sole
	     * boundary use follows the authoritative same-vertex topology.  Never
	     * apply this measured allowance in --exact mode or to a genuinely
	     * closed small loop, where it could erase real geometry. */
	    const double measured_local_tolerance = std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
	    const double geometric_endpoint_gap =
		edge.PointAtStart().DistanceTo(edge.PointAtEnd());
	    const double literal_closure_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		LocalUnits::tolerance * 1.0e-8);
	    const bool measured_open_spur = !wrapper->ImportOptions().exact &&
		edge.m_vi[0] == edge.m_vi[1] &&
		geometric_endpoint_gap > literal_closure_tolerance &&
		std::max(measured_local_tolerance, item_spur_limit) >
		    LocalUnits::tolerance;
	    const double zero_length_tolerance = measured_open_spur ?
		std::max(measured_local_tolerance, item_spur_limit) :
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
		    << " p-gap " << exposed_gap << " max-radius " << maximum_radius
		    << " tolerance " << zero_length_tolerance
		    << std::endl;
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
		    << " mm fit the bounded local/item limit "
		    << zero_length_tolerance << " mm";
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, source_edge,
		    "EDGE_CURVE", "edge_geometry", warning.str());
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


static bool
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


static void
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


static void
repair_face_bound_classification(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
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
static constexpr size_t kMaximumFaceBoundClassificationCandidates = 1024;


struct SuppliedOuterBoundCandidate {
    int loop_step_id = 0;
    std::vector<int> nonbridge_edge_step_ids;
};


struct SuppliedFaceOuterBounds {
    int face_index = -1;
    std::vector<SuppliedOuterBoundCandidate> candidates;
};


static std::vector<SuppliedFaceOuterBounds>
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


static bool
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
	if (supplied->face_index < 0 || supplied->face_index >= brep->m_F.Count()) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": supplied FACE_BOUND face index no longer exists" << std::endl;
	    return false;
	}
	const ON_BrepFace &face = brep->m_F[supplied->face_index];
	AmbiguousFace ambiguous;
	ambiguous.face_index = supplied->face_index;
	for (std::vector<SuppliedOuterBoundCandidate>::const_iterator source =
		supplied->candidates.begin();
		source != supplied->candidates.end(); ++source) {
	    int matching_loop = -1;
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count())
		    return false;
		if (brep->m_L[li].m_loop_user.i != source->loop_step_id)
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
	    }
	    /* Exact periodic-band repair combines two supplied boundary rings into
	     * one OpenNURBS loop and necessarily retains only one loop user label.
	     * Recover the other source identity from its non-bridge STEP edges. */
	    if (matching_loop < 0 && !source->nonbridge_edge_step_ids.empty()) {
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    const ON_BrepLoop &current = brep->m_L[li];
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
		}
	    }
	    if (matching_loop < 0) {
		if (wrapper->Verbose())
		{
		    std::cerr << entity_type << " #" << entity_id
			<< ": supplied STEP loop #" << source->loop_step_id
			<< " no longer belongs to face F" << supplied->face_index
			<< "; current STEP loops";
		    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
			const int li = face.m_li[fli];
			if (li >= 0 && li < brep->m_L.Count())
			    std::cerr << " L" << li << "/#"
				<< brep->m_L[li].m_loop_user.i;
		    }
		    std::cerr << "; FACE_BOUND trial is unsafe" << std::endl;
		}
		return false;
	    }
	    ambiguous.candidate_loops.push_back(matching_loop);
	}
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


static void
repair_closed_trim_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool allow_surface_alignment = false)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;

    /* Track source edges which begin this pass as an ordinary same-loop seam.
     * Verbose reports emit only phase-to-phase topology changes, making it
     * possible to locate a repair that accidentally detaches one use without
     * hard-coding corpus entity IDs or flooding normal diagnostics. */
    std::set<int> tracked_same_loop_seam_sources;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_user.i <= 0 || edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	if (first && second && first->m_li >= 0 &&
		first->m_li == second->m_li)
	    tracked_same_loop_seam_sources.insert(edge.m_edge_user.i);
    }
    const auto source_topology_signature = [brep](int source_edge_id) {
	std::ostringstream signature;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_edge_user.i != source_edge_id)
		continue;
	    signature << " E" << ei << '[' << edge.m_vi[0] << ':'
		<< edge.m_vi[1] << ']';
	    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
		const ON_BrepTrim *trim = brep->Trim(edge.m_ti[eti]);
		const ON_BrepLoop *loop = trim ? trim->Loop() : NULL;
		signature << "/T" << edge.m_ti[eti] << "(L"
		    << (trim ? trim->m_li : -1) << "/STEP"
		    << (loop ? loop->m_loop_user.i : 0) << ",type="
		    << (trim ? static_cast<int>(trim->m_type) : -1)
		    << ",rev=" << (trim && trim->m_bRev3d ? 1 : 0) << ')';
	    }
	}
	return signature.str();
    };
    std::map<int, std::string> tracked_source_signatures;
    for (std::set<int>::const_iterator source =
	    tracked_same_loop_seam_sources.begin();
	    source != tracked_same_loop_seam_sources.end(); ++source)
	tracked_source_signatures[*source] = source_topology_signature(*source);
    std::string previous_stage("initial topology");
    std::chrono::steady_clock::time_point repair_stage_started =
	std::chrono::steady_clock::now();
    bool repair_stage_active = false;
    std::map<std::string, double> previous_directed_endpoint_mismatches;
    bool have_directed_endpoint_audit = false;
    std::map<std::string, double> previous_parameter_join_mismatches;
    bool have_parameter_join_audit = false;
    const auto report_stage = [wrapper, entity_id, &entity_type,
	brep, &previous_directed_endpoint_mismatches,
	&have_directed_endpoint_audit, &previous_parameter_join_mismatches,
	&have_parameter_join_audit,
	&tracked_same_loop_seam_sources, &tracked_source_signatures,
	&source_topology_signature, &previous_stage, &repair_stage_started,
	&repair_stage_active](const char *phase) {
	const std::chrono::steady_clock::time_point now =
	    std::chrono::steady_clock::now();
	if (repair_stage_active) {
	    wrapper->RecordStageTiming("trim_orientation/" + previous_stage,
		entity_id, entity_type, static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			now - repair_stage_started).count()),
		static_cast<uint64_t>(brep->m_F.Count()),
		static_cast<uint64_t>(brep->m_E.Count()),
		static_cast<uint64_t>(brep->m_T.Count()));
	}
	repair_stage_started = now;
	repair_stage_active = true;
	if (wrapper->Verbose()) {
	    std::map<std::string, double> current_mismatches;
	    std::map<std::pair<int, int>, int> source_use_ordinals;
	    std::vector<std::pair<double, std::string> > added_details;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		const ON_BrepEdge *edge = trim.Edge();
		const ON_BrepLoop *loop = trim.Loop();
		const ON_BrepFace *face = trim.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!edge || !surface)
		    continue;
		const int loop_id = loop && loop->m_loop_user.i > 0 ?
		    loop->m_loop_user.i : -(trim.m_li + 1);
		const int edge_id = edge->m_edge_user.i > 0 ?
		    edge->m_edge_user.i : -(trim.m_ei + 1);
		const std::pair<int, int> source_key(loop_id, edge_id);
		const int use_ordinal = source_use_ordinals[source_key]++;
		std::ostringstream stable_key_stream;
		stable_key_stream << "L" << loop_id << "/E" << edge_id
		    << "/U" << use_ordinal;
		const std::string stable_key = stable_key_stream.str();
		double tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
		const ON_3dPoint start_uv = trim.PointAtStart();
		const ON_3dPoint end_uv = trim.PointAtEnd();
		const ON_3dPoint start_lift = surface->PointAt(start_uv.x,
		    start_uv.y);
		const ON_3dPoint end_lift = surface->PointAt(end_uv.x, end_uv.y);
		const ON_Interval edge_domain = edge->Domain();
		const ON_3dPoint edge_start = edge->PointAt(
		    edge_domain[trim.m_bRev3d ? 1 : 0]);
		const ON_3dPoint edge_end = edge->PointAt(
		    edge_domain[trim.m_bRev3d ? 0 : 1]);
		const double start_distance = start_lift.IsValid() &&
		    edge_start.IsValid() ? start_lift.DistanceTo(edge_start) :
		    DBL_MAX;
		const double end_distance = end_lift.IsValid() && edge_end.IsValid() ?
		    end_lift.DistanceTo(edge_end) : DBL_MAX;
		if (start_distance <= tolerance && end_distance <= tolerance)
		    continue;
		const double mismatch_ratio = std::max(start_distance,
		    end_distance) / std::max(tolerance, DBL_EPSILON);
		current_mismatches[stable_key] = mismatch_ratio;
		const std::map<std::string, double>::const_iterator previous =
		    previous_directed_endpoint_mismatches.find(stable_key);
		/* Report a new semantic use, or an existing discrepancy which becomes
		 * two orders of magnitude worse.  STEP loop/edge identities survive
		 * ON_Brep::Compact(), unlike trim array indices. */
		if (have_directed_endpoint_audit &&
			(previous == previous_directed_endpoint_mismatches.end() ||
			 mismatch_ratio > 100.0 * std::max(1.0,
			     previous->second))) {
		    std::ostringstream detail;
		    detail << ' ' << stable_key << "/T" << ti << '('
			<< start_distance << '/' << end_distance << "/tol="
			<< tolerance << ",ratio=" << mismatch_ratio << ')';
		    added_details.push_back(std::make_pair(mismatch_ratio,
			detail.str()));
		}
	    }
	    std::sort(added_details.begin(), added_details.end(),
		[](const std::pair<double, std::string> &first,
		    const std::pair<double, std::string> &second) {
		    return first.first > second.first;
		});
	if (have_directed_endpoint_audit && !added_details.empty()) {
		std::cerr << entity_type << " #" << entity_id << ": "
		    << added_details.size()
		    << " directed trim endpoint mismatch(es) appeared or worsened "
		    << "after " << previous_stage << ':';
		for (size_t added = 0; added < std::min<size_t>(8,
			added_details.size()); ++added)
		    std::cerr << added_details[added].second;
		std::cerr << std::endl;
	    }
	    previous_directed_endpoint_mismatches.swap(current_mismatches);
	    have_directed_endpoint_audit = true;

	    /* A periodic branch error can leave two trim endpoints coincident in
	     * model space while separating them by one complete surface period in
	     * parameter space.  The directed edge-locus audit above deliberately
	     * accepts that exact 3-D coincidence, but OpenNURBS still requires the
	     * cyclic p-space loop joins to be continuous.  Track those joins as a
	     * separate verbose-only stage signature so the phase which introduces
	     * a wrong periodic branch is observable without corpus-specific IDs. */
	    std::map<std::string, double> current_parameter_joins;
	    std::vector<std::pair<double, std::string> > added_parameter_details;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_BrepFace *face = loop.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!surface || loop.TrimCount() < 1)
		    continue;
		const int loop_id = loop.m_loop_user.i > 0 ?
		    loop.m_loop_user.i : -(li + 1);
		std::map<std::pair<int, int>, int> join_ordinals;
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim(
			(lti + 1) % loop.TrimCount());
		    if (!current || !next)
			continue;
		    const ON_BrepEdge *current_edge = current->Edge();
		    const ON_BrepEdge *next_edge = next->Edge();
		    const int current_source = current_edge &&
			current_edge->m_edge_user.i > 0 ?
			current_edge->m_edge_user.i : 0;
		    const int next_source = next_edge &&
			next_edge->m_edge_user.i > 0 ?
			next_edge->m_edge_user.i : 0;
		    const std::pair<int, int> edge_pair(current_source,
			next_source);
		    const int ordinal = join_ordinals[edge_pair]++;
		    std::ostringstream key_stream;
		    key_stream << "L" << loop_id << "/E" << current_source
			<< "->E" << next_source << "/U" << ordinal;
		    const std::string stable_key = key_stream.str();
		    const ON_3dPoint current_end = current->PointAtEnd();
		    const ON_3dPoint next_start = next->PointAtStart();
		    const double gap = current_end.IsValid() && next_start.IsValid() ?
			current_end.DistanceTo(next_start) : DBL_MAX;
		    if (gap <= ON_ZERO_TOLERANCE)
			continue;
		    current_parameter_joins[stable_key] = gap;
		    const std::map<std::string, double>::const_iterator previous =
			previous_parameter_join_mismatches.find(stable_key);
		    if (!have_parameter_join_audit ||
			(previous != previous_parameter_join_mismatches.end() &&
			 gap <= 100.0 * std::max(ON_ZERO_TOLERANCE,
			     previous->second)))
			continue;
		    const ON_3dPoint current_lift = surface->PointAt(
			current_end.x, current_end.y);
		    const ON_3dPoint next_lift = surface->PointAt(
			next_start.x, next_start.y);
		    const double lift_gap = current_lift.IsValid() &&
			next_lift.IsValid() ? current_lift.DistanceTo(next_lift) :
			DBL_MAX;
		    std::ostringstream detail;
		    detail << ' ' << stable_key << "/T" << current->m_trim_index
			<< "->T" << next->m_trim_index << "(uv=" << gap
			<< ",lift=" << lift_gap << ')';
		    added_parameter_details.push_back(std::make_pair(gap,
			detail.str()));
		}
	    }
	    std::sort(added_parameter_details.begin(),
		added_parameter_details.end(),
		[](const std::pair<double, std::string> &first,
		    const std::pair<double, std::string> &second) {
		    return first.first > second.first;
		});
	    if (have_parameter_join_audit && !added_parameter_details.empty()) {
		std::cerr << entity_type << " #" << entity_id << ": "
		    << added_parameter_details.size()
		    << " parameter-space loop join mismatch(es) appeared or "
		    << "worsened after " << previous_stage << ':';
		for (size_t added = 0; added < std::min<size_t>(8,
			added_parameter_details.size()); ++added)
		    std::cerr << added_parameter_details[added].second;
		std::cerr << std::endl;
	    }
	    previous_parameter_join_mismatches.swap(current_parameter_joins);
	    have_parameter_join_audit = true;
	    for (std::set<int>::const_iterator source =
		    tracked_same_loop_seam_sources.begin();
		    source != tracked_same_loop_seam_sources.end(); ++source) {
		const std::string current = source_topology_signature(*source);
		if (current == tracked_source_signatures[*source])
		    continue;
		std::cerr << entity_type << " #" << entity_id
		    << ": same-loop seam STEP edge #" << *source
		    << " topology changed after " << previous_stage << ':'
		    << tracked_source_signatures[*source] << " ->" << current
		    << std::endl;
		tracked_source_signatures[*source] = current;
	    }
	}
	previous_stage = phase;
	wrapper->SetProgressDetail(phase, entity_id, 0, 0, std::string(),
	    entity_type);
    };

    /* STEP ORIENTED_EDGE and ADVANCED_FACE senses are authoritative shell
     * topology.  Some exact periodic-band constructions must reverse a UV
     * boundary while joining formerly separate loops.  For a closed edge the
     * vertex sequence cannot reveal that this also toggled its 3-D edge use,
     * so retain the original effective sense by STEP edge identity and the
     * stable BREP face index created from the STEP face sequence.  A
     * same-face seam pair has both senses under one key and is deliberately
     * left to the seam solver. */
    typedef std::pair<int, int> StepClosedEdgeUseKey;
    std::map<StepClosedEdgeUseKey, std::set<bool> > expected_closed_edge_uses;
    report_stage("indexing authoritative closed STEP edge uses");
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	const ON_BrepTrim &trim = brep->m_T[ti];
	const ON_BrepEdge *edge = trim.Edge();
	const ON_BrepFace *face = trim.Face();
	if (!edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
		trim.m_vi[0] != trim.m_vi[1] || edge->m_edge_user.i <= 0 ||
		face->m_face_index < 0)
	    continue;
	expected_closed_edge_uses[StepClosedEdgeUseKey(edge->m_edge_user.i,
	    face->m_face_index)].insert(trim.m_bRev3d ^ face->m_bRev);
    }

    /* Moving a periodic surface seam is exact, but it can disturb an already
     * valid choice of parameter branches elsewhere on the same surface.  Keep
     * it out of the ordinary repair pass and retry with seam alignment only
     * when the bounded repairs below still leave structurally invalid
     * topology. */
    /* Topology inference must not trust an unvalidated periodic branch.  In
     * particular, a half-circle may arrive with a pcurve on the complementary
     * half of a closed rational revolution surface; its endpoints still lift
     * correctly, but its interior does not follow the authoritative 3-D STEP
     * edge.  Repair those exact edge loci before interpreting net pcurve
     * winding as an implicit band or pole cut. */
    report_stage("materializing exact supplied periodic boundary cuts");
    split_supplied_full_period_boundaries(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    report_stage("validating exact pcurves before periodic topology inference");
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type, true);
    if (brlcad::PullbackWorkCancelled())
	return;

    /* Resolve integral-period choices on already complete STEP loops before
     * looking for compact implicit bands.  A closed rational revolution
     * patch can have an ordinary rectangular four-edge boundary whose exact
     * pcurves use both 0 and 2*pi.  Treating its one net winding as a missing
     * band seam first triggers unnecessary edge pullback and can construct
     * the wrong cap topology. */
    report_stage("solving initial coherent exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    report_stage("repairing implicit exact periodic face bands");
    repair_implicit_periodic_face_bands(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    report_stage("classifying exact STEP face bounds");
    repair_face_bound_classification(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;

    std::vector<ON_BrepLoop::TYPE> expected_loop_types;
    expected_loop_types.reserve(brep->m_L.Count());
    report_stage("capturing exact BREP loop classifications");
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
	expected_loop_types.push_back(brep->m_L[li].m_type);
    }
    std::vector<int> aligned_surface_loops;
    report_stage("repairing bounded periodic seam classifications");
    repair_bounded_seam_isos(brep, wrapper, entity_id, entity_type,
	allow_surface_alignment, &aligned_surface_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing adjacent exact BREP topology vertices");
    repair_adjacent_trim_vertices(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing invalid exact open pcurves");
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing ordinary closed-surface seam crossings");
    if (repair_closed_surface_seam_crossings(brep, wrapper, entity_id,
	    entity_type))
	repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing paired exact seam boundaries");
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing missing exact singular trims");
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("selecting exact singular-pole periodic branches");
    repair_ambiguous_singular_periodic_loop_branches(brep, wrapper,
	entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("solving coherent exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("regenerating exact periodic loop chains");
    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("resolving regenerated exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing adjacent exact periodic trim endpoints");
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing zero-length exact boundary edges");
    repair_zero_length_boundary_edges(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Pole cuts are ordinary paired seams even when their surface seam was
     * aligned during initial trim construction rather than this repair pass.
     * Include every seam loop which still has a model-space exact p-space gap
     * in the cyclic branch solve; restricting this to freshly aligned surfaces
     * leaves otherwise valid pole loops one whole period open. */
    const auto periodic_branch_loops = [brep, &aligned_surface_loops]() {
	std::set<int> selected(aligned_surface_loops.begin(),
	    aligned_surface_loops.end());
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    const ON_BrepLoop &loop = brep->m_L[li];
	    const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	    if (!surface || loop.TrimCount() < 2)
		continue;
	    bool has_seam = false;
	    bool exact_parameter_gap = false;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *current = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!current || !next)
		    continue;
		has_seam = has_seam || current->m_type == ON_BrepTrim::seam ||
		    next->m_type == ON_BrepTrim::seam;
		const ON_3dPoint current_end = current->PointAtEnd();
		const ON_3dPoint next_start = next->PointAtStart();
		if (current_end.DistanceTo(next_start) <= ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint current_lift = surface->PointAt(
		    current_end.x, current_end.y);
		const ON_3dPoint next_lift = surface->PointAt(
		    next_start.x, next_start.y);
		if (current_lift.IsValid() && next_lift.IsValid() &&
			current_lift.DistanceTo(next_lift) <= LocalUnits::tolerance)
		    exact_parameter_gap = true;
	    }
	    if (has_seam && exact_parameter_gap)
		selected.insert(li);
	}
	return std::vector<int>(selected.begin(), selected.end());
    };
    std::vector<int> branch_loops = periodic_branch_loops();
    if (!branch_loops.empty()) {
	report_stage("solving exact paired-seam loop branches");
	repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	    branch_loops);
	repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
    }

    /* A surface-alignment retry starts from the completed bounded repairs.
     * Relocating a periodic seam translates pcurves without changing their
     * 3-D lifts or tangent direction, so the dense closed-edge orientation
     * proof remains valid and must not be repeated. */
    if (!allow_surface_alignment) {
    report_stage("validating exact closed-edge trim orientations");
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	if ((ti & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
	    trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
	    trim.m_vi[0] != trim.m_vi[1])
	    continue;
	const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	if (edge.m_vi[0] != edge.m_vi[1])
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	if (!surface)
	    continue;
	const double orientation_tolerance = std::max(LocalUnits::tolerance,
	    std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1])));

	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs))
	    continue;
	const ON_Interval trim_domain = trim.Domain();
	int matching_samples = 0;
	int opposing_samples = 0;
	for (int sample = 1; sample < 16; ++sample) {
	    if (brlcad::PullbackWorkCancelled())
		return;
	    const double normalized = static_cast<double>(sample) / 16.0;
	    ON_3dPoint uv;
	    ON_3dVector uv_tangent;
	    if (!trim.Ev1Der(trim_domain.ParameterAt(normalized), uv, uv_tangent))
		continue;
	    ON_3dPoint lifted_point;
	    ON_3dVector du, dv;
	    if (!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
		continue;
	    ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    if (!lifted_tangent.Unitize())
		continue;
	    /* The trim/edge use supplies a directed parameter correspondence.  If
	     * its exact edge point already proves locus membership, a global NURBS
	     * closest-point solve cannot improve the acceptance result.  Retain the
	     * optimizer only for legitimately reparameterized pcurves whose direct
	     * correspondence falls outside the model tolerance. */
	    double edge_parameter = edge_nurbs.Domain().ParameterAt(
		trim.m_bRev3d ? 1.0 - normalized : normalized);
	    double edge_distance = lifted_point.DistanceTo(
		edge_nurbs.PointAt(edge_parameter));
	    /* This pass only gathers evidence for changing orientation.  A global
	     * NURBS closest-point solve here used to run as many as fifteen times
	     * per closed trim and has no bounded/cancellable openNURBS interface.
	     * When the directed STEP correspondence is outside tolerance, treating
	     * the sample as inconclusive is both safer and substantially cheaper:
	     * endpoint proof and the densely validated regeneration path below
	     * remain available, while an unrelated closest locus cannot be used as
	     * evidence for reversing an edge use. */
	    if (edge_distance > orientation_tolerance)
		continue;
	    ON_3dVector edge_tangent = edge_nurbs.TangentAt(edge_parameter);
	    if (!edge_tangent.Unitize())
		continue;
	    double alignment = lifted_tangent * edge_tangent;
	    if (trim.m_bRev3d) alignment = -alignment;
	    if (alignment > 0.5)
		++matching_samples;
	    else if (alignment < -0.5)
		++opposing_samples;
	}
	double endpoint_alignment[2];
	closed_trim_endpoint_alignments(trim, edge, surface, endpoint_alignment);
	const bool endpoint_alignment_valid[2] = {
	    closed_trim_endpoint_alignment_is_valid(endpoint_alignment[0]),
	    closed_trim_endpoint_alignment_is_valid(endpoint_alignment[1])
	};
	const bool negative_endpoint_alignment[2] = {
	    endpoint_alignment_valid[0] && endpoint_alignment[0] < 0.0,
	    endpoint_alignment_valid[1] && endpoint_alignment[1] < 0.0
	};
	if (negative_endpoint_alignment[0] && negative_endpoint_alignment[1] &&
		trim.PointAtStart().DistanceTo(trim.PointAtEnd()) <= ON_ZERO_TOLERANCE) {
	    ON_Curve *reversed = trim.DuplicateCurve();
	    bool valid_reversal = reversed && reversed->Reverse() &&
		reversed->ChangeDimension(2) && reversed->IsValid() &&
		reversed->PointAtStart().DistanceTo(trim.PointAtStart()) <=
		    ON_ZERO_TOLERANCE &&
		reversed->PointAtEnd().DistanceTo(trim.PointAtEnd()) <=
		    ON_ZERO_TOLERANCE;
	    const ON_Interval reversed_domain = reversed ? reversed->Domain() :
		ON_Interval::EmptyInterval;
	    for (int end = 0; valid_reversal && end < 2; ++end) {
		ON_3dPoint uv, lifted_point;
		ON_3dVector uv_tangent, du, dv;
		if (!reversed->Ev1Der(reversed_domain[end], uv, uv_tangent) ||
			!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv)) {
		    valid_reversal = false;
		    break;
		}
		ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
		ON_3dVector edge_tangent = edge.TangentAt(edge.Domain()[
		    trim.m_bRev3d ? 1 - end : end]);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		    valid_reversal = false;
		    break;
		}
		double alignment = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d)
		    alignment = -alignment;
		valid_reversal = alignment >= 0.0;
	    }
	    for (int sample = 0; valid_reversal &&
		    sample <= kDenseValidationSegments; ++sample) {
		if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
		    delete reversed;
		    return;
		}
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = reversed->PointAt(
		    reversed_domain.ParameterAt(fraction));
		const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
		valid_reversal = lifted.IsValid();
		if (valid_reversal) {
		    const ON_Interval edge_domain = edge_nurbs.Domain();
		    const ON_3dPoint forward = edge_nurbs.PointAt(
			edge_domain.ParameterAt(fraction));
		    const ON_3dPoint reverse = edge_nurbs.PointAt(
			edge_domain.ParameterAt(1.0 - fraction));
		    /* Reversal validation needs geometric-locus membership, not a
		     * particular parameterization.  Either directed correspondence is
		     * a sufficient exact-edge witness; consult the global optimizer only
		     * when both direct witnesses fail. */
		    double locus_distance = std::min(lifted.DistanceTo(forward),
			lifted.DistanceTo(reverse));
		    if (locus_distance > orientation_tolerance) {
			double closest_parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&closest_parameter,
				&edge_nurbs, lifted))
			    locus_distance = std::min(locus_distance,
				lifted.DistanceTo(edge_nurbs.PointAt(
				    closest_parameter)));
		    }
		    valid_reversal = locus_distance <= orientation_tolerance;
		}
	    }
	    if (valid_reversal) {
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index >= 0 && brep->SetTrimCurve(trim, c2_index)) {
		    brep->SetTrimIsoFlags(trim);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"reversed a closed pcurve whose endpoint tangents opposed its edge use");
		    continue;
		}
		if (c2_index < 0)
		    delete reversed;
	    } else {
		delete reversed;
	    }
	}
	if (endpoint_alignment_valid[0] && endpoint_alignment_valid[1] &&
		endpoint_alignment[0] < -0.9 && endpoint_alignment[1] < -0.9 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected a closed-edge trim orientation proven at both endpoints");
	    continue;
	}
	/* A tangent at a collapsed pole can oppose the well-conditioned interior
	 * solely because the surface derivative is singular there.  Do not launch
	 * the expensive dense regeneration when at least five interior samples all
	 * prove the existing orientation.  Regeneration remains mandatory for a
	 * negative endpoint accompanied by mixed, opposing, or insufficient
	 * interior evidence. */
	const bool interior_proves_current_orientation =
	    matching_samples >= 5 && opposing_samples == 0;
	std::string regeneration_failure;
	if ((negative_endpoint_alignment[0] || negative_endpoint_alignment[1]) &&
		!interior_proves_current_orientation &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    orientation_tolerance, &regeneration_failure, NULL, NULL, NULL,
		    false, wrapper)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"regenerated a closed-edge pcurve with consistent endpoint tangents");
	    continue;
	}
	if (wrapper->Verbose() && !regeneration_failure.empty())
	    std::cerr << entity_type << " #" << entity_id << ": closed trim " << ti
		<< " pcurve regeneration rejected: " << regeneration_failure << std::endl;
	/* The interior correspondence must demonstrate one direction consistently.
	 * Mixed or weak samples indicate a kink, self-intersection, or ambiguous
	 * closest point and are deliberately left for validation to reject. */
	if (wrapper->Verbose() && (negative_endpoint_alignment[0] ||
		negative_endpoint_alignment[1] || opposing_samples > 0 ||
		matching_samples + opposing_samples < 5)) {
	    std::cerr << entity_type << " #" << entity_id << ": closed trim " << ti
		<< " (loop " << trim.m_li << ", edge " << trim.m_ei
		<< ", reversed " << (trim.m_bRev3d ? "yes" : "no")
		<< ", loop trims " << (trim.Loop() ? trim.Loop()->TrimCount() : 0)
		<< ") orientation samples matching=" << matching_samples
		<< ", opposing=" << opposing_samples
		<< ", endpoint dots=" << endpoint_alignment[0] << ","
		<< endpoint_alignment[1] << ", pcurve closure="
		<< trim.PointAtStart().DistanceTo(trim.PointAtEnd()) << std::endl;
	}
	if (opposing_samples >= 5 && matching_samples == 0 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected closed-edge trim orientation");
	}
    }
    }

    /* Closed-edge regeneration above can replace a pcurve after the ordinary
     * endpoint pass.  Reclose only the newly exposed, model-space exact joins
     * before recomputing derived topology state. */
    report_stage("reclosing joins after exact closed-edge repair");
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_ambiguous_singular_periodic_loop_branches(brep, wrapper,
	entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    branch_loops = periodic_branch_loops();
    if (!branch_loops.empty())
	repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	    branch_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Surface-seam alignment and whole-period loop unwrapping can expose a
     * seam pair that was valid when first installed but no longer agrees with
     * the final shared surface parameterization.  Reconcile exact seam pairs
     * once more without permitting another surface seam move, then re-run the
     * bounded open-curve and join checks against that stable surface. */
    report_stage("reconciling final exact periodic seam pairs");
    repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type, NULL);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Pairwise endpoint sweeps can oscillate when independently supplied
     * periodic boundary edges disagree within a previously measured source
     * tolerance.  Close both uses of their shared seam transactionally after
     * all edge regeneration has settled. */
    repair_paired_seam_loop_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    report_stage("refreshing exact OpenNURBS trim topology");
	/* Curve replacement and orientation repair invalidate derived p-space
	 * state.  Refresh it once, after all bounded repairs, using the same
	 * routines ON_Brep::IsValid() uses.  Do not call ON_Brep::FlipLoop merely
	 * because ComputeLoopType() changes: FlipLoop also reverses loop order,
	 * every trim pcurve, and every m_bRev3d edge use.  That would replace the
	 * authoritative STEP oriented-edge topology with a derived 2-D area
	 * classification and can make an otherwise consistent closed shell
	 * non-orientable. */
    refresh_brep_flags_preserving_singular_isos(brep, true, wrapper,
	entity_id, &entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
	report_stage("restoring exact periodic loop orientations");
	/* Reversing a p-space loop also toggles every trim's 3-D edge use.  It is
	 * appropriate when exact periodic reconstruction reversed a complete loop,
	 * but unsafe when only ComputeLoopType() chose a different periodic branch.
	 * Predict the completed shell consequence and accept a reversal only when
	 * it does not increase the number of shared-edge orientation conflicts. */
	/* Only edge uses belonging to the candidate loop can change when that
	 * loop is flipped.  The former implementation recomputed the complete
	 * shell and closed-STEP-use conflict counts before and after every
	 * candidate, making this pass quadratic in the number of loops.  Large
	 * imported assemblies can contain tens of thousands of loops in one
	 * representation.  Compare the affected edge/trim subset instead; the
	 * unchanged remainder contributes the same value to both totals. */
	const auto loop_shell_orientation_conflicts = [brep](int toggled_loop,
		size_t &before, size_t &after) {
	    before = 0;
	    after = 0;
	    if (toggled_loop < 0 || toggled_loop >= brep->m_L.Count())
		return;
	    std::set<int> affected_edges;
	    const ON_BrepLoop &loop = brep->m_L[toggled_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		if (trim && trim->m_ei >= 0)
		    affected_edges.insert(trim->m_ei);
	    }
	    for (std::set<int>::const_iterator eit = affected_edges.begin();
		    eit != affected_edges.end(); ++eit) {
		if (*eit < 0 || *eit >= brep->m_E.Count())
		    continue;
		const ON_BrepEdge &edge = brep->m_E[*eit];
		if (edge.m_ti.Count() != 2)
		    continue;
		const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
		const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
		const ON_BrepFace *first_face = first ? first->Face() : NULL;
		const ON_BrepFace *second_face = second ? second->Face() : NULL;
		if (!first || !second || !first_face || !second_face ||
			first_face == second_face)
		    continue;
		const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
		const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
		before += first_effective == second_effective;
		const bool first_after = first->m_li == toggled_loop ?
		    !first_effective : first_effective;
		const bool second_after = second->m_li == toggled_loop ?
		    !second_effective : second_effective;
		after += first_after == second_after;
	    }
	};
	const auto loop_closed_step_use_conflicts = [brep,
		&expected_closed_edge_uses](int toggled_loop, size_t &before,
		    size_t &after) {
	    before = 0;
	    after = 0;
	    if (toggled_loop < 0 || toggled_loop >= brep->m_L.Count())
		return;
	    const ON_BrepLoop &loop = brep->m_L[toggled_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		const ON_BrepFace *face = trim ? trim->Face() : NULL;
		if (!trim || !edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
			trim->m_vi[0] != trim->m_vi[1] ||
			edge->m_edge_user.i <= 0 || face->m_face_index < 0)
		    continue;
		const std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator
		    expected = expected_closed_edge_uses.find(StepClosedEdgeUseKey(
			edge->m_edge_user.i, face->m_face_index));
		if (expected == expected_closed_edge_uses.end() ||
			expected->second.size() != 1)
		    continue;
		const bool effective = trim->m_bRev3d ^ face->m_bRev;
		before += effective != *expected->second.begin();
		after += !effective != *expected->second.begin();
	    }
	};
	bool loop_orientation_changed = false;
	std::set<StepClosedEdgeUseKey> shell_orientation_overrides;
	for (int li = 0; li < brep->m_L.Count() &&
		static_cast<size_t>(li) < expected_loop_types.size(); ++li) {
	    if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
		return;
	    const ON_BrepLoop::TYPE expected = expected_loop_types[li];
	    ON_BrepLoop &loop = brep->m_L[li];
	    if ((expected != ON_BrepLoop::outer &&
		    expected != ON_BrepLoop::inner) ||
		    (loop.m_type != ON_BrepLoop::outer &&
		     loop.m_type != ON_BrepLoop::inner) || expected == loop.m_type)
		continue;
	    size_t before = 0;
	    size_t after = 0;
	    size_t step_before = 0;
	    size_t step_after = 0;
	    loop_shell_orientation_conflicts(li, before, after);
	    loop_closed_step_use_conflicts(li, step_before, step_after);
	    /* A strict reduction in reciprocal shell conflicts is a global proof
	     * that the local STEP FACE_BOUND/ORIENTED_EDGE combination is
	     * inconsistent.  In that one case closed-shell orientability outranks
	     * the original edge-use bit; otherwise never increase authoritative
	     * STEP-use disagreement. */
	    const bool shell_orientation_improves = after < before;
	    if (after > before ||
		    (step_after > step_before && !shell_orientation_improves)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": declined loop " << li << "/STEP"
			<< loop.m_loop_user.i << " orientation reversal because "
			<< "shared-edge conflicts would change " << before << "->"
			<< after << " and authoritative closed STEP edge-use "
			<< "conflicts " << step_before << "->" << step_after
			<< std::endl;
		continue;
	    }
	    brep->FlipLoop(loop);
	    loop_orientation_changed = true;
	    if (shell_orientation_improves) {
		/* The trial proves the relationship between both uses of every
		 * affected edge, not just the use in the loop being flipped.  If
		 * later STEP-sense restoration is suppressed only on this side, it
		 * can toggle the reciprocal closed-edge use and recreate exactly the
		 * shell conflict the trial removed.  Protect both proven uses as one
		 * constraint. */
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (!edge || edge->m_edge_user.i <= 0)
			continue;
		    for (int eti = 0; eti < edge->m_ti.Count(); ++eti) {
			const ON_BrepTrim *edge_trim = brep->Trim(edge->m_ti[eti]);
			const ON_BrepFace *edge_face = edge_trim ?
			    edge_trim->Face() : NULL;
			if (!edge_trim || !edge_face || edge_face->m_face_index < 0)
			    continue;
			const StepClosedEdgeUseKey key(edge->m_edge_user.i,
			    edge_face->m_face_index);
			if (expected_closed_edge_uses.find(key) !=
				expected_closed_edge_uses.end())
			    shell_orientation_overrides.insert(key);
		    }
		}
	    }
	    wrapper->RecordRepair(entity_id, entity_type, "loop_orientation",
		shell_orientation_improves ?
		"corrected a demonstrably inconsistent STEP loop orientation from closed-shell edge-use constraints" :
		"restored a periodic loop orientation without increasing closed-shell edge-use conflicts");
	}
	if (loop_orientation_changed)
	    refresh_brep_flags_preserving_singular_isos(brep, true, wrapper,
		entity_id, &entity_type);
	/* The refresh above may still report an unknown loop type on a periodic
	 * face, or a different winding from a periodic branch choice.  Preserve
	 * the authoritative FACE_OUTER_BOUND/FACE_BOUND classification (or the
	 * unambiguous pre-repair classification recovered above), including slit
	 * loops proven by ComputeLoopType(); derived loop flags are not allowed to
	 * erase it. */
    for (int li = 0; li < brep->m_L.Count() &&
	static_cast<size_t>(li) < expected_loop_types.size(); ++li) {
	if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
	const ON_BrepLoop::TYPE expected = expected_loop_types[li];
	if (expected == ON_BrepLoop::outer || expected == ON_BrepLoop::inner ||
		expected == ON_BrepLoop::slit)
	    brep->m_L[li].m_type = expected;
	}
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    if ((fi & 63) == 0 && brlcad::PullbackWorkCancelled())
		return;
	    brep->SortFaceLoops(brep->m_F[fi]);
	}
	report_stage("checking final exact closed-trim orientations");
	/* Endpoint repair can alter the endpoint tangent of a closed pcurve.
	 * Perform the final exact tangent check after all such repairs, then
	 * refresh the derived trim state once more. */
	if (!allow_surface_alignment)
	    repair_final_closed_trim_orientations(brep, wrapper, entity_id,
		entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	report_stage("restoring authoritative closed STEP edge uses");
	size_t restored_step_edge_uses = 0;
	for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	    ON_BrepTrim &trim = brep->m_T[ti];
	    const ON_BrepEdge *edge = trim.Edge();
	    const ON_BrepFace *face = trim.Face();
	    if (!edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
		    trim.m_vi[0] != trim.m_vi[1] || edge->m_edge_user.i <= 0 ||
		    face->m_face_index < 0)
		continue;
	    const std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator
		expected = expected_closed_edge_uses.find(StepClosedEdgeUseKey(
		    edge->m_edge_user.i, face->m_face_index));
	    if (expected == expected_closed_edge_uses.end() ||
		    expected->second.size() != 1 ||
		    shell_orientation_overrides.find(expected->first) !=
			shell_orientation_overrides.end())
		continue;
	    const bool desired = *expected->second.begin();
	    if ((trim.m_bRev3d ^ face->m_bRev) == desired)
		continue;

	    /* A bounded repair may have regenerated a closed pcurve in the
	     * direction which makes its lifted tangents agree with the 3-D edge,
	     * then corrected m_bRev3d accordingly.  Restoring the authoritative
	     * STEP edge-use sense by changing only m_bRev3d would make those
	     * tangents oppose the edge and fails ON_Brep::IsValid().  Closed trim
	     * endpoints cannot distinguish the two directions, so use the same
	     * endpoint-tangent proof as openNURBS and reverse the exact pcurve
	     * together with the flag when necessary.  Reversal preserves the curve
	     * locus; the coincident endpoints preserve loop connectivity. */
	    double alignment[2];
	    closed_trim_endpoint_alignments(trim, *edge, face->SurfaceOf(),
		alignment);
	    const bool toggle_would_oppose =
		closed_trim_endpoint_alignment_is_valid(alignment[0]) &&
		closed_trim_endpoint_alignment_is_valid(alignment[1]) &&
		alignment[0] > 0.0 && alignment[1] > 0.0;
	    if (toggle_would_oppose) {
		ON_Curve *reversed = trim.DuplicateCurve();
		const ON_3dPoint original_start = trim.PointAtStart();
		const ON_3dPoint original_end = trim.PointAtEnd();
		if (!reversed || !reversed->Reverse() ||
			!reversed->ChangeDimension(2) || !reversed->IsValid() ||
			reversed->PointAtStart().DistanceTo(original_start) >
			    ON_ZERO_TOLERANCE ||
			reversed->PointAtEnd().DistanceTo(original_end) >
			    ON_ZERO_TOLERANCE) {
		    delete reversed;
		    continue;
		}
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
		    if (c2_index < 0)
			delete reversed;
		    continue;
		}
		brep->SetTrimIsoFlags(trim);
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "reversed a regenerated closed pcurve while restoring its authoritative STEP edge-use sense");
	    }
	    trim.m_bRev3d = !trim.m_bRev3d;
	    ++restored_step_edge_uses;
	}
	for (size_t restored = 0; restored < restored_step_edge_uses; ++restored)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"restored an authoritative closed STEP edge-use orientation after periodic loop repair");
	report_stage("restoring authoritative split STEP edge-use chains");

	/* Splitting a closed STEP edge opens each child at the periodic seam, so the
	 * closed-trim restoration above no longer recognizes it.  Retain the same
	 * authoritative pre-repair sense for the complete child chain.  Periodic
	 * loop joining may embed that chain as one closed lobe of a larger keyhole
	 * loop; reversing the whole face or whole loop would disturb unrelated
	 * exact edges.  Reverse only a single contiguous, closed, all-mismatching
	 * source-edge subchain.  Reversing its pcurves, order, and edge-use flags
	 * together preserves every 3-D locus and both attachment points. */
	size_t restored_split_step_edge_uses = 0;
	std::unique_ptr<ON_Brep> split_step_use_rollback;
	std::set<int> split_step_use_faces;
	for (std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator expected =
		expected_closed_edge_uses.begin();
		expected != expected_closed_edge_uses.end(); ++expected) {
	    if (expected->second.size() != 1 ||
		    shell_orientation_overrides.find(expected->first) !=
			shell_orientation_overrides.end())
		continue;
	    const int source_edge_id = expected->first.first;
	    const int face_index = expected->first.second;
	    size_t split_edge_count = 0;
	    size_t conflicting_split_edges = 0;
	    bool complete_reciprocal_split = true;
	    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
		const ON_BrepEdge &source_edge = brep->m_E[ei];
		if (source_edge.m_edge_user.i != source_edge_id)
		    continue;
		++split_edge_count;
		if (source_edge.m_ti.Count() != 2) {
		    complete_reciprocal_split = false;
		    continue;
		}
		const ON_BrepTrim *first = brep->Trim(source_edge.m_ti[0]);
		const ON_BrepTrim *second = brep->Trim(source_edge.m_ti[1]);
		const ON_BrepFace *first_face = first ? first->Face() : NULL;
		const ON_BrepFace *second_face = second ? second->Face() : NULL;
		if (!first || !second || !first_face || !second_face ||
			first_face == second_face) {
		    complete_reciprocal_split = false;
		    continue;
		}
		const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
		const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
		conflicting_split_edges += first_effective == second_effective;
	    }
	    /* Authoritative sense restoration is warranted only for the exact
	     * failure it repairs: every child of one closed STEP edge is a
	     * reciprocal two-face use and every child currently has equal effective
	     * directions.  A merely duplicated source id is not evidence that a
	     * valid periodic boundary should be reversed. */
	    if (split_edge_count < 2 || !complete_reciprocal_split ||
		    conflicting_split_edges != split_edge_count)
		continue;

	    int loop_index = -1;
	    std::vector<int> selected_offsets;
	    bool all_mismatch = true;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		const ON_BrepEdge *edge = trim.Edge();
		const ON_BrepFace *face = trim.Face();
		if (!edge || !face || edge->m_edge_user.i != source_edge_id ||
			face->m_face_index != face_index)
		    continue;
		if (loop_index < 0)
		    loop_index = trim.m_li;
		if (trim.m_li != loop_index ||
			(trim.m_bRev3d ^ face->m_bRev) == *expected->second.begin()) {
		    all_mismatch = false;
		    break;
		}
		const ON_BrepLoop *loop = trim.Loop();
		if (!loop) {
		    all_mismatch = false;
		    break;
		}
		for (int lti = 0; lti < loop->TrimCount(); ++lti)
		    if (loop->m_ti[lti] == ti) {
			selected_offsets.push_back(lti);
			break;
		    }
	    }
	    if (!all_mismatch || loop_index < 0 ||
		    loop_index >= brep->m_L.Count() ||
		    selected_offsets.size() != split_edge_count)
		continue;

	    ON_BrepLoop &loop = brep->m_L[loop_index];
	    std::vector<unsigned char> selected(
		static_cast<size_t>(loop.TrimCount()), 0);
	    for (std::vector<int>::const_iterator offset = selected_offsets.begin();
		    offset != selected_offsets.end(); ++offset) {
		if (*offset < 0 || *offset >= loop.TrimCount()) {
		    all_mismatch = false;
		    break;
		}
		selected[static_cast<size_t>(*offset)] = 1;
	    }
	    if (!all_mismatch)
		continue;
	    int chain_start = -1;
	    int chain_starts = 0;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		if (selected[static_cast<size_t>(lti)] &&
			!selected[static_cast<size_t>((lti + loop.TrimCount() - 1) %
			    loop.TrimCount())]) {
		    chain_start = lti;
		    ++chain_starts;
		}
	    }
	    if (selected_offsets.size() == static_cast<size_t>(loop.TrimCount())) {
		chain_start = 0;
		chain_starts = 1;
	    }
	    if (chain_starts != 1)
		continue;

	    std::vector<int> chain_offsets;
	    std::vector<int> chain_trims;
	    for (size_t member = 0; member < selected_offsets.size(); ++member) {
		const int offset = (chain_start + static_cast<int>(member)) %
		    loop.TrimCount();
		if (!selected[static_cast<size_t>(offset)]) {
		    all_mismatch = false;
		    break;
		}
		chain_offsets.push_back(offset);
		chain_trims.push_back(loop.m_ti[offset]);
	    }
	    if (!all_mismatch || chain_trims.empty())
		continue;
	    bool closed_chain = true;
	    for (size_t member = 0; member < chain_trims.size(); ++member) {
		const ON_BrepTrim *current = brep->Trim(chain_trims[member]);
		const ON_BrepTrim *next = brep->Trim(
		    chain_trims[(member + 1) % chain_trims.size()]);
		if (!current || !next || current->m_vi[1] != next->m_vi[0]) {
		    closed_chain = false;
		    break;
		}
	    }
	    if (!closed_chain)
		continue;

	    /* The first accepted reversal starts one transaction covering all
	     * split-edge sense restorations.  Reversing a closed child chain is
	     * topologically exact, but on a periodic face its coincident topology
	     * vertex can have two distinct UV images.  Later branch regeneration
	     * must not be allowed to exchange those images and open a face loop. */
	    if (!split_step_use_rollback)
		split_step_use_rollback.reset(new ON_Brep(*brep));
	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    bool reversed_chain = true;
	    for (std::vector<int>::const_iterator trim_index = chain_trims.begin();
		    trim_index != chain_trims.end(); ++trim_index) {
		ON_BrepTrim &trim = brep->m_T[*trim_index];
		ON_Curve *reversed = trim.DuplicateCurve();
		if (!reversed || !reversed->Reverse() ||
			!reversed->ChangeDimension(2) || !reversed->IsValid()) {
		    delete reversed;
		    reversed_chain = false;
		    break;
		}
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
		    if (c2_index < 0)
			delete reversed;
		    reversed_chain = false;
		    break;
		}
		trim.m_bRev3d = !trim.m_bRev3d;
		std::swap(trim.m_vi[0], trim.m_vi[1]);
	    }
	    if (reversed_chain) {
		for (size_t member = 0; member < chain_offsets.size(); ++member)
		    loop.m_ti[chain_offsets[member]] =
			chain_trims[chain_trims.size() - member - 1];
		std::string unsafe_topology;
		reversed_chain = brep_topology_references_are_safe(brep,
		    &unsafe_topology);
	    }
	    if (!reversed_chain) {
		*brep = *rollback;
		continue;
	    }
	    split_step_use_faces.insert(face_index);
	    ++restored_split_step_edge_uses;
	}
	if (restored_split_step_edge_uses) {
	    /* Reversing a closed lobe preserves its surface lift but exchanges the
	     * periodic images at its two attachment points.  Reconstruct the whole
	     * affected loop from its already detached exact edges so adjacent UVs
	     * select one coherent unwrapped branch. */
	    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
	    if (brlcad::PullbackWorkCancelled()) return;
	    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	    if (brlcad::PullbackWorkCancelled()) return;

	    /* Compare only the faces touched by this transaction.  The surrounding
	     * BREP may contain an independent source-invalid face, so whole-BREP
	     * validity is not a usable rollback criterion here.  Preserve any
	     * pre-existing invalidity, but reject a newly invalid affected face or
	     * any increase in its cyclic parameter-space join gaps. */
	    const auto face_join_gaps = [](const ON_Brep &candidate, int fi,
		    size_t &count, double &maximum) {
		count = 0;
		maximum = 0.0;
		if (fi < 0 || fi >= candidate.m_F.Count())
		    return false;
		const ON_BrepFace &face = candidate.m_F[fi];
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= candidate.m_L.Count())
			return false;
		    const ON_BrepLoop &loop = candidate.m_L[li];
		    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *current = loop.Trim(lti);
			const ON_BrepTrim *next = loop.Trim(
			    (lti + 1) % loop.TrimCount());
			if (!current || !next)
			    return false;
			const ON_3dPoint end = current->PointAtEnd();
			const ON_3dPoint start = next->PointAtStart();
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
	    bool split_restoration_valid = split_step_use_rollback.get() != NULL;
	    for (std::set<int>::const_iterator fi = split_step_use_faces.begin();
		    split_restoration_valid && fi != split_step_use_faces.end(); ++fi) {
		if (*fi < 0 || *fi >= split_step_use_rollback->m_F.Count() ||
			*fi >= brep->m_F.Count()) {
		    split_restoration_valid = false;
		    break;
		}
		ON_wString before_messages;
		ON_TextLog before_log(before_messages);
		ON_wString after_messages;
		ON_TextLog after_log(after_messages);
		const bool before_valid =
		    split_step_use_rollback->m_F[*fi].IsValid(&before_log);
		const bool after_valid = brep->m_F[*fi].IsValid(&after_log);
		size_t before_gap_count = 0;
		size_t after_gap_count = 0;
		double before_maximum_gap = 0.0;
		double after_maximum_gap = 0.0;
		const bool before_audited = face_join_gaps(
		    *split_step_use_rollback, *fi, before_gap_count,
		    before_maximum_gap);
		const bool after_audited = face_join_gaps(*brep, *fi,
		    after_gap_count, after_maximum_gap);
		const bool join_regression = !before_audited || !after_audited ||
		    after_gap_count > before_gap_count ||
		    after_maximum_gap > std::max(100.0 * ON_ZERO_TOLERANCE,
			100.0 * before_maximum_gap);
		if ((before_valid && !after_valid) || join_regression)
		    split_restoration_valid = false;
	    }
	    if (!split_restoration_valid) {
		*brep = *split_step_use_rollback;
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information, entity_id,
		    entity_type, "trim_orientation",
		    "declined split closed-edge sense restoration because it "
		    "degraded an exact periodic face loop");
		restored_split_step_edge_uses = 0;
	    }
	}
	for (size_t restored = 0; restored < restored_split_step_edge_uses;
		restored++)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"restored an authoritative closed STEP edge-use orientation across an exact split subchain");
	report_stage("refreshing final exact seam topology");
	/* The completed STEP face-bound graph is authoritative when an exporter
	 * omitted FACE_OUTER_BOUND.  The final closed-edge tangent pass does not
	 * reverse pcurves, so refreshing loop types here would only discard the
	 * unambiguous bound classification restored above. */
	refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
	    entity_id, &entity_type);
	/* The generic OpenNURBS refresh derives seam ISO and trim type together.
	 * On an edge with additional uses elsewhere in the BREP it can downgrade
	 * one member of an otherwise exact same-loop seam pair to x_iso/y_iso.
	 * Re-run the transactional exact-edge seam construction in the final
	 * position so its densely validated complementary boundary flags are not
	 * overwritten before structural validation. */
	repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type);
	/* Loop-orientation restoration can exchange the periodic image of one seam
	 * member after the earlier endpoint pass.  Reconcile the seam and all four
	 * adjacent endpoints transactionally here, after the final generic flag
	 * refresh, so no later branch propagation can put both uses on one side. */
	repair_paired_seam_loop_endpoints(brep, wrapper, entity_id, entity_type);
	/* Rebuilding that final seam pair can deliberately move it to the native
	 * opposite boundaries after the earlier loop-wide endpoint sweep.  Its
	 * ordinary neighbours may consequently remain on the old, lift-equivalent
	 * periodic image.  Give those non-seam curves one final integral-period
	 * translation and densely validated endpoint reconciliation before
	 * structural validation. */
	repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	report_stage("validating completed bounded trim repair");
	if (!allow_surface_alignment) {
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    if (!brep->IsValid(&validation_log)) {
		bool unresolved_open_surface_join = false;
		for (int li = 0; li < brep->m_L.Count() &&
			!unresolved_open_surface_join; ++li) {
		    const ON_BrepLoop &loop = brep->m_L[li];
		    const ON_Surface *surface = loop.Face() ?
			loop.Face()->SurfaceOf() : NULL;
		    if (!surface)
			continue;
		    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *current = loop.Trim(lti);
			const ON_BrepTrim *next = loop.Trim(
			    (lti + 1) % loop.TrimCount());
			if (!current || !next) continue;
			const ON_3dPoint current_end = current->PointAtEnd();
			const ON_3dPoint next_start = next->PointAtStart();
			for (int direction = 0; direction < 2; ++direction) {
			    if (fabs(current_end[direction] - next_start[direction]) >
				    ON_ZERO_TOLERANCE && !surface->IsClosed(direction)) {
				unresolved_open_surface_join = true;
				break;
			    }
			}
			if (unresolved_open_surface_join) break;
		    }
		}
		if (wrapper->Verbose()) {
		    ON_String validation_text(validation_messages);
		    std::cerr << entity_type << " #" << entity_id
			<< ": bounded repair pass remained structurally invalid before "
			   "periodic surface alignment retry:\n"
			<< validation_text.Array();
		}
		/* A periodic surface seam move cannot alter an unresolved join on an
		 * open surface.  Repeating every repair on a full BREP in that case is
		 * both deterministic wasted work and can consume the entire item
		 * budget before the actionable open-surface diagnostic is emitted. */
		if (unresolved_open_surface_join) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": skipping periodic surface alignment retry because an "
			       "open-surface p-space join remains unresolved" << std::endl;
		    return;
		}
		std::unique_ptr<ON_Brep> bounded_repairs(new ON_Brep(*brep));
		/* Apply seam relocation to the already completed bounded repairs.
		 * Restarting from retry_source repeated every dense regeneration and
		 * routinely exhausted an item's budget before validation. */
		repair_closed_trim_orientations(brep, wrapper, entity_id,
		    entity_type, true);
		ON_wString aligned_messages;
		ON_TextLog aligned_log(aligned_messages);
	if (brep->IsValid(&aligned_log)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"selected a valid retry with exact periodic surface seam alignment");
	    return;
	}
	if (wrapper->Verbose()) {
	    ON_String aligned_text(aligned_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": periodic surface seam retry validation failed:\n"
		<< aligned_text.Array();
	}
	/* Seam alignment is exact, but it is a whole-surface operation and can
		 * disturb unrelated periodic branches.  Never discard the ordinary
		 * bounded repairs unless the retry actually validates. */
		*brep = *bounded_repairs;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic surface seam retry remained invalid; "
			<< "retaining the bounded pcurve repairs" << std::endl;
	    }
	}
	if (repair_stage_active) {
	    const std::chrono::steady_clock::time_point now =
		std::chrono::steady_clock::now();
	    wrapper->RecordStageTiming("trim_orientation/" + previous_stage,
		entity_id, entity_type, static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			now - repair_stage_started).count()),
		static_cast<uint64_t>(brep->m_F.Count()),
		static_cast<uint64_t>(brep->m_E.Count()),
		static_cast<uint64_t>(brep->m_T.Count()));
	    repair_stage_active = false;
	}
	if (wrapper->Verbose()) {
	    int pspace_gap_diagnostics = 0;
	    for (int li = 0; li < brep->m_L.Count() && pspace_gap_diagnostics < 16; ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
		if (!surface)
		    continue;
		for (int lti = 0; lti < loop.TrimCount() && pspace_gap_diagnostics < 16;
		     ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		    if (!current || !next)
			continue;
		    const ON_3dPoint current_uv = current->PointAtEnd();
		    const ON_3dPoint next_uv = next->PointAtStart();
		    const double pspace_gap = current_uv.DistanceTo(next_uv);
		    if (pspace_gap <= ON_ZERO_TOLERANCE)
			continue;
		    const ON_3dPoint current_lift = surface->PointAt(
			current_uv.x, current_uv.y);
		    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		    double current_vertex_distance = DBL_MAX;
		    double next_vertex_distance = DBL_MAX;
		    if (current->m_vi[1] >= 0 && current->m_vi[1] < brep->m_V.Count())
			current_vertex_distance = current_lift.DistanceTo(
			    brep->m_V[current->m_vi[1]].point);
		    if (next->m_vi[0] >= 0 && next->m_vi[0] < brep->m_V.Count())
			next_vertex_distance = next_lift.DistanceTo(
			    brep->m_V[next->m_vi[0]].point);
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< "/STEP" << loop.m_loop_user.i << " p-space gap "
			<< current->m_trim_index << "(type="
			<< static_cast<int>(current->m_type) << ",iso="
			<< static_cast<int>(current->m_iso) << ",e=" << current->m_ei
			<< ",rev=" << (current->m_bRev3d ? 1 : 0) << ",v="
			<< current->m_vi[1] << ")->" << next->m_trim_index
			<< "(type=" << static_cast<int>(next->m_type) << ",iso="
			<< static_cast<int>(next->m_iso) << ",e=" << next->m_ei
			<< ",rev=" << (next->m_bRev3d ? 1 : 0) << ",v="
			<< next->m_vi[0]
			<< ") uv=" << pspace_gap << " lift="
			<< current_lift.DistanceTo(next_lift) << " vertex distances="
			<< current_vertex_distance << '/' << next_vertex_distance
			<< " domains=" << surface->Domain(0).Min() << ':'
			<< surface->Domain(0).Max() << ',' << surface->Domain(1).Min()
			<< ':' << surface->Domain(1).Max() << " closed="
			<< (surface->IsClosed(0) ? 1 : 0)
			<< (surface->IsClosed(1) ? 1 : 0)
			<< std::endl;
		    ++pspace_gap_diagnostics;
		}
	    }
	    size_t trim_vertex_mismatches = 0;
	    const size_t trim_vertex_diagnostic_limit = 16;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		    if (!current || !next || current->m_vi[1] == next->m_vi[0])
			continue;
		    ++trim_vertex_mismatches;
		    if (trim_vertex_mismatches > trim_vertex_diagnostic_limit)
			continue;
		    const ON_BrepEdge *current_edge = current->Edge();
		    const ON_BrepEdge *next_edge = next->Edge();
		    double vertex_distance = DBL_MAX;
		    if (current->m_vi[1] >= 0 && current->m_vi[1] < brep->m_V.Count() &&
			next->m_vi[0] >= 0 && next->m_vi[0] < brep->m_V.Count())
			vertex_distance = brep->m_V[current->m_vi[1]].point.DistanceTo(
			    brep->m_V[next->m_vi[0]].point);
		    const ON_3dPoint current_uv = current->PointAtEnd();
		    const ON_3dPoint next_uv = next->PointAtStart();
		    double lift_distance = DBL_MAX;
		    if (surface) {
			const ON_3dPoint current_lift = surface->PointAt(current_uv.x,
			    current_uv.y);
			const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
			if (current_lift.IsValid() && next_lift.IsValid())
			    lift_distance = current_lift.DistanceTo(next_lift);
		    }
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< " (STEP loop " << loop.m_loop_user.i << ") trim vertex mismatch "
			<< current->m_trim_index << "[" << current->m_vi[0] << ','
			<< current->m_vi[1] << "]/edge " << current->m_ei << "/STEP"
			<< (current_edge ? current_edge->m_edge_user.i : 0) << " -> "
			<< next->m_trim_index << "[" << next->m_vi[0] << ','
			<< next->m_vi[1] << "]/edge " << next->m_ei << "/STEP"
			<< (next_edge ? next_edge->m_edge_user.i : 0)
			<< " vertex distance=" << vertex_distance
			<< " p-space/lift distance=" << current_uv.DistanceTo(next_uv)
			<< '/' << lift_distance << std::endl;
		}
	    }
	    if (trim_vertex_mismatches > trim_vertex_diagnostic_limit)
		std::cerr << entity_type << " #" << entity_id << ": suppressed "
		    << trim_vertex_mismatches - trim_vertex_diagnostic_limit
		    << " additional trim vertex mismatches ("
		    << trim_vertex_mismatches << " total)" << std::endl;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim *first = brep->Trim(ti);
		const ON_BrepEdge *edge = first ? first->Edge() : NULL;
		if (!first || !edge || first->m_type != ON_BrepTrim::seam)
		    continue;
		const ON_BrepTrim *second = NULL;
		for (int use = 0; use < edge->m_ti.Count(); ++use) {
		    const ON_BrepTrim *candidate = brep->Trim(edge->m_ti[use]);
		    if (candidate && candidate != first &&
			    candidate->m_type == ON_BrepTrim::seam &&
			    candidate->m_li == first->m_li) {
			second = candidate;
			break;
		    }
		}
		if (!second || first->m_trim_index > second->m_trim_index)
		    continue;
		const bool complementary =
		    (first->m_iso == ON_Surface::W_iso &&
		     second->m_iso == ON_Surface::E_iso) ||
		    (first->m_iso == ON_Surface::E_iso &&
		     second->m_iso == ON_Surface::W_iso) ||
		    (first->m_iso == ON_Surface::S_iso &&
		     second->m_iso == ON_Surface::N_iso) ||
		    (first->m_iso == ON_Surface::N_iso &&
		     second->m_iso == ON_Surface::S_iso);
		if (complementary)
		    continue;
		const ON_BoundingBox first_box = first->BoundingBox();
		const ON_BoundingBox second_box = second->BoundingBox();
		const ON_Surface *surface = first->Face() ?
		    first->Face()->SurfaceOf() : NULL;
		std::cerr << entity_type << " #" << entity_id
		    << ": mismatched seam edge " << first->m_ei << " trims "
		    << first->m_trim_index << '/' << second->m_trim_index
		    << " iso=" << static_cast<int>(first->m_iso) << '/'
		    << static_cast<int>(second->m_iso) << " uv-boxes="
		    << first_box.m_min.x << ':' << first_box.m_max.x << ','
		    << first_box.m_min.y << ':' << first_box.m_max.y << " / "
		    << second_box.m_min.x << ':' << second_box.m_max.x << ','
		    << second_box.m_min.y << ':' << second_box.m_max.y;
		if (surface)
		    std::cerr << " domains=" << surface->Domain(0).Min() << ':'
			<< surface->Domain(0).Max() << ','
			<< surface->Domain(1).Min() << ':'
			<< surface->Domain(1).Max();
		std::cerr << std::endl;
	    }
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		if (trim.m_type == ON_BrepTrim::seam &&
			trim.m_iso != ON_Surface::W_iso && trim.m_iso != ON_Surface::E_iso &&
			trim.m_iso != ON_Surface::S_iso && trim.m_iso != ON_Surface::N_iso) {
		    const ON_BoundingBox box = trim.BoundingBox();
		    const ON_BrepLoop *loop = trim.Loop();
		    const ON_Surface *surface = loop && loop->Face() ?
			loop->Face()->SurfaceOf() : NULL;
		    std::cerr << entity_type << " #" << entity_id
			<< ": unresolved seam after bounded repair trim " << ti
			<< " iso=" << static_cast<int>(trim.m_iso) << " uv box="
			<< box.m_min.x << ':' << box.m_max.x << ','
			<< box.m_min.y << ':' << box.m_max.y;
		    if (surface)
			std::cerr << " surface closed=" << (surface->IsClosed(0) ? '1' : '0')
			    << (surface->IsClosed(1) ? '1' : '0') << " domains="
			    << surface->Domain(0).Min() << ':' << surface->Domain(0).Max()
			    << ',' << surface->Domain(1).Min() << ':'
			    << surface->Domain(1).Max();
		    if (trim.m_ei >= 0 && trim.m_ei < brep->m_E.Count()) {
			const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
			std::cerr << " edge=" << trim.m_ei << " uses=";
			for (int use = 0; use < edge.m_ti.Count(); ++use)
			    std::cerr << (use ? "," : "") << edge.m_ti[use];
		    }
		    std::cerr << std::endl;
		}
	    }
	}
}


struct TopologicalKeyholeSplit {
    int original_loop = -1;
    int detached_loop = -1;
    int face = -1;
};


/* STEP permits an edge to be used forward and backward as a zero-area bridge
 * from an outer boundary to another closed boundary component.  OpenNURBS
 * calls two uses of one edge in one loop a seam and consequently requires
 * native N/E/S/W pcurves.  For a non-isoparametric bridge, its own source-edge
 * identity proves exact cancellation; split the two remaining topology cycles
 * and discard only the opposite bridge uses.  This helper is intentionally
 * candidate-only: the caller retains the result only if bounded pcurve repair
 * and complete OpenNURBS structural validation subsequently succeed. */
static bool
split_topological_keyhole_candidate(ON_Brep *brep,
    TopologicalKeyholeSplit *split)
{
    if (split)
	*split = TopologicalKeyholeSplit();
    if (!brep || !split)
	return false;

    const auto boundary_iso = [](ON_Surface::ISO iso) {
	return iso == ON_Surface::W_iso || iso == ON_Surface::E_iso ||
	    iso == ON_Surface::S_iso || iso == ON_Surface::N_iso;
    };
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop *loop = &brep->m_L[li];
	ON_BrepFace *face = loop->Face();
	const int trim_count = loop->TrimCount();
	/* Restrict the transactional retry to one source outer loop.  Assigning
	 * pre-existing holes between two newly separated faces would require a
	 * containment decision outside this exact topology normalization. */
	if (!face || face->LoopCount() != 1 ||
		loop->m_type != ON_BrepLoop::outer || trim_count < 4)
	    continue;
	for (int first_offset = 0; first_offset < trim_count; ++first_offset) {
	    ON_BrepTrim *first = loop->Trim(first_offset);
	    if (!first || first->m_type != ON_BrepTrim::seam ||
		    boundary_iso(first->m_iso) || first->m_ei < 0 ||
		    first->m_ei >= brep->m_E.Count())
		continue;
	    const ON_BrepEdge &edge = brep->m_E[first->m_ei];
	    if (edge.m_ti.Count() != 2)
		continue;
	    for (int second_offset = first_offset + 2;
		    second_offset < trim_count; ++second_offset) {
		if (first_offset == 0 && second_offset == trim_count - 1)
		    continue;
		ON_BrepTrim *second = loop->Trim(second_offset);
		if (!second || second->m_type != ON_BrepTrim::seam ||
			second->m_ei != first->m_ei ||
			second->m_bRev3d == first->m_bRev3d)
		    continue;

		std::vector<int> inside;
		std::vector<int> outside;
		for (int offset = first_offset + 1; offset < second_offset; ++offset)
		    inside.push_back(loop->m_ti[offset]);
		for (int offset = second_offset + 1; offset < trim_count; ++offset)
		    outside.push_back(loop->m_ti[offset]);
		for (int offset = 0; offset < first_offset; ++offset)
		    outside.push_back(loop->m_ti[offset]);
		if (inside.empty() || outside.empty())
		    continue;
		/* The shared edge supplies the bridge endpoints.  Each exposed chain
		 * must already close at the same immutable topology vertex; only its
		 * p-space image is eligible for the bounded repair retry. */
		if (brep->m_T[inside.back()].m_vi[1] !=
			brep->m_T[inside.front()].m_vi[0] ||
			brep->m_T[outside.back()].m_vi[1] !=
			brep->m_T[outside.front()].m_vi[0])
		    continue;

		const int face_index = face->m_face_index;
		const int first_trim_index = first->m_trim_index;
		const int second_trim_index = second->m_trim_index;
		const int detached_loop_index =
		    brep->NewLoop(ON_BrepLoop::unknown).m_loop_index;
		loop = &brep->m_L[li];
		ON_BrepLoop *detached = &brep->m_L[detached_loop_index];
		loop->m_ti.SetCount(0);
		for (std::vector<int>::const_iterator trim = outside.begin();
			trim != outside.end(); ++trim) {
		    loop->m_ti.Append(*trim);
		    brep->m_T[*trim].m_li = li;
		}
		for (std::vector<int>::const_iterator trim = inside.begin();
			trim != inside.end(); ++trim) {
		    detached->m_ti.Append(*trim);
		    brep->m_T[*trim].m_li = detached_loop_index;
		}
		/* Preserve the authoritative FACE_OUTER_BOUND classification on the
		 * source chain.  The detached chain has no independent STEP bound and
		 * must earn its type from its repaired pcurves. */
		loop->m_type = ON_BrepLoop::outer;
		loop->m_fi = face_index;
		detached->m_type = ON_BrepLoop::unknown;
		detached->m_fi = face_index;
		brep->m_F[face_index].m_li.Append(detached_loop_index);

		brep->m_T[first_trim_index].m_li = -1;
		brep->m_T[second_trim_index].m_li = -1;
		const int higher_trim = std::max(first_trim_index, second_trim_index);
		const int lower_trim = std::min(first_trim_index, second_trim_index);
		brep->DeleteTrim(brep->m_T[higher_trim], false);
		brep->DeleteTrim(brep->m_T[lower_trim], true);
		if (!brep->Compact())
		    return false;
		split->original_loop = li;
		split->detached_loop = detached_loop_index;
		split->face = face_index;
		return true;
	    }
	}
    }
    return false;
}


static bool
retry_topological_keyhole_normalization(ON_Brep *brep, STEPWrapper *wrapper,
    int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return false;
    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
    std::vector<TopologicalKeyholeSplit> splits;
    for (int attempt = 0; attempt < 32; ++attempt) {
	refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
	TopologicalKeyholeSplit split;
	if (!split_topological_keyhole_candidate(candidate.get(), &split))
	    break;
	splits.push_back(split);
    }
    if (splits.empty()) {
	if (wrapper->Verbose()) {
	    for (int ti = 0; ti < candidate->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = candidate->m_T[ti];
		if (trim.m_type != ON_BrepTrim::seam ||
			trim.m_iso == ON_Surface::W_iso ||
			trim.m_iso == ON_Surface::E_iso ||
			trim.m_iso == ON_Surface::S_iso ||
			trim.m_iso == ON_Surface::N_iso || trim.m_li < 0 ||
			trim.m_li >= candidate->m_L.Count())
		    continue;
		const ON_BrepLoop &loop = candidate->m_L[trim.m_li];
		const ON_BrepFace *face = loop.Face();
		std::cerr << entity_type << " #" << entity_id
		    << ": topological keyhole candidate trim " << ti
		    << " not selected (loop " << trim.m_li << " type "
		    << static_cast<int>(loop.m_type) << " trims "
		    << loop.TrimCount() << ", face loops "
		    << (face ? face->LoopCount() : 0) << ", edge uses "
		    << ((trim.m_ei >= 0 && trim.m_ei < candidate->m_E.Count()) ?
			candidate->m_E[trim.m_ei].m_ti.Count() : 0) << ')'
		    << std::endl;
		break;
	    }
	}
	return false;
    }
    const auto reject = [wrapper, entity_id, &entity_type](const char *reason) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole normalization retry rejected: "
		<< reason << std::endl;
	return false;
    };

    repair_closed_trim_orientations(candidate.get(), wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return reject("the item work budget expired");
    /* The repair pass has already computed the detached loop type and restored
     * the authoritative source FACE_OUTER_BOUND type.  Refreshing loop types
     * again here would erase that source classification on a periodic face. */
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);

    for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	if (split->original_loop < 0 ||
		split->original_loop >= candidate->m_L.Count() ||
		split->detached_loop < 0 ||
		split->detached_loop >= candidate->m_L.Count() ||
		split->face < 0 || split->face >= candidate->m_F.Count())
	    return reject("a split topology index changed unexpectedly");
	ON_BrepLoop &original = candidate->m_L[split->original_loop];
	ON_BrepLoop &detached = candidate->m_L[split->detached_loop];
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole split loops "
		<< split->original_loop << '/' << split->detached_loop
		<< " acquired types " << static_cast<int>(original.m_type) << '/'
		<< static_cast<int>(detached.m_type) << std::endl;
	if ((original.m_type != ON_BrepLoop::outer &&
		original.m_type != ON_BrepLoop::inner) ||
		(detached.m_type != ON_BrepLoop::outer &&
		 detached.m_type != ON_BrepLoop::inner))
	    return reject("a separated boundary did not acquire an outer/inner type");
	if (original.m_type == ON_BrepLoop::outer &&
		detached.m_type == ON_BrepLoop::outer) {
	    ON_BrepFace &source_face = candidate->m_F[split->face];
	    int detached_offset = -1;
	    for (int fli = 0; fli < source_face.m_li.Count(); ++fli)
		if (source_face.m_li[fli] == split->detached_loop) {
		    detached_offset = fli;
		    break;
		}
	    if (detached_offset < 0)
		return reject("the detached loop was absent from its source face");
	    const int surface_index = source_face.m_si;
	    const bool reversed = source_face.m_bRev;
	    source_face.m_li.Remove(detached_offset);
	    ON_BrepFace &new_face = candidate->NewFace(surface_index);
	    new_face.m_bRev = reversed;
	    new_face.m_li.Append(split->detached_loop);
	    candidate->m_L[split->detached_loop].m_fi = new_face.m_face_index;
	} else if (original.m_type != ON_BrepLoop::outer &&
		detached.m_type != ON_BrepLoop::outer) {
	    return reject("neither separated boundary was an outer loop");
	}
    }
    for (int fi = 0; fi < candidate->m_F.Count(); ++fi)
	candidate->SortFaceLoops(candidate->m_F[fi]);
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
    repair_face_bound_classification(candidate.get(), wrapper, entity_id,
	entity_type);

    ON_wString candidate_messages;
    ON_TextLog candidate_log(candidate_messages);
    if (!candidate->IsValid(&candidate_log)) {
	if (wrapper->Verbose()) {
	    ON_String text(candidate_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole normalization retry rejected:\n"
		<< text.Array();
	}
	return reject("the structurally complete separated BREP remained invalid");
    }
    *brep = *candidate;
    for (size_t split = 0; split < splits.size(); ++split)
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "removed an exact opposite-use STEP keyhole bridge after validated topology separation");
    return true;
}
