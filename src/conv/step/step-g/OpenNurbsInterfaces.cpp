/*                 OpenNurbsInterfaces.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/OpenNurbsInterfaces.cpp
 *
 * Routines to convert STEP "OpenNurbsInterfaces" to BRL-CAD BREP
 * structures.
 *
 */

#include "common.h"

#include "brep/defines.h"
#include "brep/pullback.h"

#include "sdai.h"
class SDAI_Application_instance;

/* must come after nist step headers */
#include "brep.h"

#include <map>
#include "nmg.h"

#include "STEPEntity.h"
#include "Axis1Placement.h"
#include "Factory.h"
#include "LocalUnits.h"
#include "PullbackCurve.h"
#include "Point.h"
#include "CartesianPoint.h"
#include "VertexPoint.h"
#include "Vector.h"
#include "EdgeCurve.h"
#include "OrientedEdge.h"

// Curve includes
#include "BezierCurve.h"
#include "BSplineCurve.h"
#include "BSplineCurveWithKnots.h"
#include "QuasiUniformCurve.h"
#include "RationalBezierCurve.h"
#include "RationalBSplineCurve.h"
#include "RationalBSplineCurveWithKnots.h"
#include "RationalQuasiUniformCurve.h"
#include "RationalUniformCurve.h"
#include "UniformCurve.h"

// Surface includes
#include "Line.h"
#include "Circle.h"
#include "Ellipse.h"
#include "Hyperbola.h"
#include "Parabola.h"
#include "CylindricalSurface.h"
#include "ConicalSurface.h"
#include "SweptSurface.h"
#include "SurfaceOfLinearExtrusion.h"
#include "SurfaceOfRevolution.h"
#include "Path.h"
#include "Plane.h"
#include "Loop.h"
#include "VertexLoop.h"
#include "Face.h"
#include "OpenShell.h"
#include "OrientedFace.h"
#include "FaceBound.h"
#include "FaceOuterBound.h"
#include "FaceSurface.h"
#include "BezierSurface.h"
#include "BSplineSurface.h"
#include "BSplineSurfaceWithKnots.h"
#include "QuasiUniformSurface.h"
#include "RationalBezierSurface.h"
#include "RationalBSplineSurface.h"
#include "RationalBSplineSurfaceWithKnots.h"
#include "RationalQuasiUniformSurface.h"
#include "RationalUniformSurface.h"
#include "SphericalSurface.h"
#include "ToroidalSurface.h"
#include "UniformSurface.h"

#include "AdvancedBrepShapeRepresentation.h"
#include "ShellBasedSurfaceModel.h"
#include "PullbackCurve.h"

#include "brep.h"

//#define _DEBUG_TESTING_
#ifdef _DEBUG_TESTING_
extern void print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints);
#endif

namespace {

/* Dense validation is the final proof that a repaired or reused pcurve lifts
 * to its exact STEP edge within the file uncertainty.  1024 uniform segments
 * were selected as a bounded empirical budget: they resolve the short seam
 * reversals in the Mark V acceptance model while keeping validation linear and
 * deterministic.  This is a validation budget, not a geometric tolerance or
 * a request to approximate the edge with 1024 segments. */
constexpr int kDenseLiftValidationSegments = 1024;

/* Keep numerical solver floors comfortably above floating-point zero without
 * replacing the model-derived tolerance used for acceptance. */
constexpr double kNumericalToleranceScale = 1024.0;

/* A relocated closed-surface seam needs a real parameter-space interval that
 * contains no sampled boundary.  Requiring one thousandth of the period keeps
 * the seam away from sampling noise while still admitting narrow exact faces;
 * every relocated pcurve is subsequently checked against its 3-D edge. */
constexpr double kMinimumSafeSeamGapFraction = 1.0e-3;

} // namespace


static double
distance_to_curve(const ON_Curve *curve, const ON_3dPoint &point)
{
    if (!curve)
	return DBL_MAX;

    ON_NurbsCurve nurbs;
    if (!curve->GetNurbForm(nurbs))
	return DBL_MAX;
    double parameter = 0.0;
    if (!ON_NurbsCurve_GetClosestPoint(&parameter, &nurbs, point))
	return DBL_MAX;
    return point.DistanceTo(nurbs.PointAt(parameter));
}


static void
destroy_pullback_data(PBCData *data)
{
    if (!data)
	return;
    if (data->segments) {
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
    }
    delete data;
}


static int
pullback_sample_count(const PBCData *data)
{
    int count = 0;
    if (!data || !data->segments)
	return count;
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (*segment)
	    count += (*segment)->Count();
    }
    return count;
}


static double
short_curve_pullback_resolution(const ON_Curve *curve, double model_tolerance)
{
    const double numerical_floor = ON_ZERO_TOLERANCE * kNumericalToleranceScale;
    if (!curve || !(model_tolerance > numerical_floor))
	return std::max(numerical_floor, model_tolerance * 0.1);

    ON_BoundingBox bbox;
    double feature_size = 0.0;
    if (curve->GetTightBoundingBox(bbox, false, NULL) && bbox.IsValid())
	feature_size = bbox.Diagonal().Length();
    const ON_Interval domain = curve->Domain();
    feature_size = std::max(feature_size,
	curve->PointAt(domain.Min()).DistanceTo(curve->PointAt(domain.Max())));
    if (!(feature_size > numerical_floor))
	return std::max(numerical_floor, model_tolerance * 0.1);

    /* The file uncertainty remains the validation and topology tolerance.
     * This smaller value only prevents the numerical closest-point solver
     * from treating every point of a sub-tolerance feature as the same UV. */
    return std::max(numerical_floor,
	std::min(model_tolerance * 0.1, feature_size * 0.01));
}


static bool
refine_surface_point_seeded(const ON_Surface *surface, const ON_3dPoint &target,
	double tolerance, ON_3dPoint &uv, double *final_distance)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint point;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, point, du, dv))
	    break;
	const ON_3dVector residual = target - point;
	best_distance = residual.Length();
	if (best_distance <= tolerance)
	    break;
	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = DBL_EPSILON * metric_scale * 64.0;
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
		if (surface->IsClosed(direction))
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
    return best_distance <= tolerance;
}


static bool
seam_boundary_score(PBCData *data, int direction, double value, double tolerance, double *score)
{
    if (!data || !data->surf || !data->curve || !data->segments || !score || tolerance <= 0.0)
	return false;

    const ON_Interval domain = data->surf->Domain(direction);
    if (!domain.IsIncreasing())
	return false;

    *score = 0.0;
    for (std::list<ON_2dPointArray *>::const_iterator segment = data->segments->begin();
	 segment != data->segments->end(); ++segment) {
	if (!*segment)
	    continue;
	for (int i = 0; i < (*segment)->Count(); ++i) {
	    const ON_2dPoint original = (**segment)[i];
	    ON_2dPoint snapped = original;
	    snapped[direction] = value;
	    const ON_3dPoint lifted = data->surf->PointAt(snapped.x, snapped.y);
	    if (!lifted.IsValid() || distance_to_curve(data->curve, lifted) > tolerance)
		return false;
	    *score += fabs(original[direction] - value) / domain.Length();
	}
    }
    return true;
}


static void
pullback_loop_metrics(const std::list<PBCData *> &pullbacks, PBCData *first, double first_value,
	PBCData *second, double second_value, int direction, double period, double *area, double *gap)
{
    ON_2dPoint initial;
    ON_2dPoint previous;
    bool have_previous = false;
    double twice_area = 0.0;
    *gap = 0.0;
    for (std::list<PBCData *>::const_iterator data = pullbacks.begin(); data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	for (std::list<ON_2dPointArray *>::const_iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (!*segment)
		continue;
	    for (int i = 0; i < (*segment)->Count(); ++i) {
		ON_2dPoint point = (**segment)[i];
		if (*data == first)
		    point[direction] = first_value;
		else if (*data == second)
		    point[direction] = second_value;
		else if (have_previous)
		    point[direction] += round((previous[direction] - point[direction]) / period) * period;
		if (!have_previous) {
		    initial = point;
		    have_previous = true;
		} else {
		    if (i == 0) {
			const double join_gap = previous.DistanceTo(point);
			if (join_gap > *gap)
			    *gap = join_gap;
		    }
		    twice_area += previous.x * point.y - point.x * previous.y;
		}
		previous = point;
	    }
	}
    }
    if (have_previous) {
	twice_area += previous.x * initial.y - initial.x * previous.y;
	const double closure_gap = previous.DistanceTo(initial);
	if (closure_gap > *gap)
	    *gap = closure_gap;
    }
    *area = 0.5 * twice_area;
}


static bool
align_nurbs_surface_seam(std::list<PBCData *> &pullbacks, const ON_Surface *surface,
	double tolerance)
{
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(const_cast<ON_Surface *>(surface));
    if ((!nurbs && !revolution) || !(tolerance > 0.0))
	return false;

    for (std::list<PBCData *>::iterator first = pullbacks.begin(); first != pullbacks.end(); ++first) {
	if (!*first || !(*first)->edge || !(*first)->segments)
	    continue;
	std::list<PBCData *>::iterator second = first;
	++second;
	for (; second != pullbacks.end(); ++second) {
	    if (!*second || (*second)->edge != (*first)->edge || !(*second)->segments)
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		if (!(period > 0.0))
		    continue;
		double values[2] = {0.0, 0.0};
		bool constant[2] = {true, true};
		PBCData *pair[2] = {*first, *second};
		for (int member = 0; member < 2; ++member) {
		    bool have_value = false;
		    for (std::list<ON_2dPointArray *>::const_iterator segment =
			    pair[member]->segments->begin();
			 segment != pair[member]->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point) {
			    const double value = (**segment)[point][direction];
			    if (!have_value) {
				values[member] = value;
				have_value = true;
			    } else if (fabs(value - values[member]) >
				    1.0e-8 * std::max(1.0, period)) {
				constant[member] = false;
			    }
			}
		    }
		    if (!have_value)
			constant[member] = false;
		}
		if (!constant[0] || !constant[1] ||
			fabs(fabs(values[0] - values[1]) - period) >
			    1.0e-7 * std::max(1.0, period))
		    continue;

		const double lower = std::min(values[0], values[1]);
		double seam = std::max(values[0], values[1]);
		while (seam < domain.Min()) seam += period;
		while (seam > domain.Max()) seam -= period;
		if (seam <= domain.Min() + ON_ZERO_TOLERANCE ||
			seam >= domain.Max() - ON_ZERO_TOLERANCE)
		    continue;
		const double shift = seam - lower;
		ON_NurbsSurface nurbs_candidate;
		ON_RevSurface revolution_candidate;
		const ON_Surface *candidate = NULL;
		if (nurbs) {
		    nurbs_candidate = *nurbs;
		    if (!nurbs_candidate.ChangeSurfaceSeam(direction, seam))
			continue;
		    candidate = &nurbs_candidate;
		} else {
		    const int angle_direction = revolution->m_bTransposed ? 1 : 0;
		    if (direction != angle_direction ||
			fabs(revolution->m_angle.Length() - ON_2PI) > ON_ZERO_TOLERANCE)
			continue;
		    revolution_candidate = *revolution;
		    const double angle = revolution->m_angle.ParameterAt(
			domain.NormalizedParameterAt(seam));
		    revolution_candidate.m_angle.Set(angle, angle + ON_2PI);
		    revolution_candidate.m_t.Set(seam, seam + period);
		    candidate = &revolution_candidate;
		}

		bool valid = true;
		for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		     valid && data != pullbacks.end(); ++data) {
		    if (!*data || !(*data)->segments)
			continue;
		    for (std::list<ON_2dPointArray *>::const_iterator segment =
			    (*data)->segments->begin();
			 valid && segment != (*data)->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point) {
			    const ON_2dPoint original = (**segment)[point];
			    ON_2dPoint shifted = original;
			    shifted[direction] += shift;
			    const ON_3dPoint original_lift = surface->PointAt(
				original.x, original.y);
			    const ON_3dPoint shifted_lift = candidate->PointAt(
				shifted.x, shifted.y);
			    if (!original_lift.IsValid() || !shifted_lift.IsValid() ||
				    original_lift.DistanceTo(shifted_lift) > tolerance) {
				valid = false;
				break;
			    }
			}
		    }
		}
		if (!valid)
		    continue;

		if (nurbs)
		    *nurbs = nurbs_candidate;
		else
		    *revolution = revolution_candidate;
		for (std::list<PBCData *>::iterator data = pullbacks.begin();
		     data != pullbacks.end(); ++data) {
		    if (!*data || !(*data)->segments)
			continue;
		    for (std::list<ON_2dPointArray *>::iterator segment =
			    (*data)->segments->begin();
			 segment != (*data)->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point)
			    (**segment)[point][direction] += shift;
		    }
		}
		return true;
	    }
	}
    }
    return false;
}


/* Move a closed NURBS seam out of an ordinary face boundary.  STEP exporters
 * are allowed to place a face across the underlying surface seam without
 * supplying a duplicated topological seam edge.  In that case independently
 * wrapped pullbacks produce a full-period jump even though every 3-D edge is
 * exact.  Choose the largest sampled boundary-free interval, change the
 * surface seam there, and remap each sample to the new domain.  This is an
 * exact reparameterization: no sample is accepted unless its lift is unchanged
 * within model uncertainty. */
static bool
relocate_nurbs_surface_seam_away_from_boundary(
    std::list<PBCData *> &pullbacks, const ON_Surface *surface, double tolerance)
{
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(surface));
    if (!nurbs || !(tolerance > 0.0) || pullbacks.empty())
	return false;

    std::set<const ON_BrepEdge *> edges;
    for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	 data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->edge || !edges.insert((*data)->edge).second)
	    return false; /* A repeated edge is a topological seam, handled above. */
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval old_domain = surface->Domain(direction);
	const double period = old_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

	std::vector<double> parameters;
	bool crosses_current_seam = false;
	bool have_previous = false;
	double previous = 0.0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		 ++segment) {
		if (!*segment)
		    continue;
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    const double value = (**segment)[point][direction];
		    if (!std::isfinite(value)) {
			parameters.clear();
			break;
		    }
		    double phase = std::fmod(value - old_domain.Min(), period);
		    if (phase < 0.0) phase += period;
		    parameters.push_back(phase);
		    if (have_previous && fabs(value - previous) > 0.5 * period)
			crosses_current_seam = true;
		    previous = value;
		    have_previous = true;
		}
		if (parameters.empty()) break;
	    }
	    if (parameters.empty()) break;
	}
	if (!crosses_current_seam || parameters.size() < 2)
	    continue;

	std::sort(parameters.begin(), parameters.end());
	double largest_gap = -1.0;
	double gap_start = 0.0;
	for (size_t index = 0; index < parameters.size(); ++index) {
	    const double next = index + 1 < parameters.size() ?
		parameters[index + 1] : parameters[0] + period;
	    const double gap = next - parameters[index];
	    if (gap > largest_gap) {
		largest_gap = gap;
		gap_start = parameters[index];
	    }
	}
	if (largest_gap < kMinimumSafeSeamGapFraction * period)
	    continue;
	double seam_phase = std::fmod(gap_start + 0.5 * largest_gap, period);
	if (seam_phase < 0.0) seam_phase += period;
	const double seam = old_domain.Min() + seam_phase;
	const double endpoint_guard = std::max(ON_ZERO_TOLERANCE,
	    1.0e-10 * period);
	if (seam <= old_domain.Min() + endpoint_guard ||
		seam >= old_domain.Max() - endpoint_guard)
	    continue;

	ON_NurbsSurface candidate(*nurbs);
	if (!candidate.ChangeSurfaceSeam(direction, seam) || !candidate.IsValid())
	    continue;
	const ON_Interval new_domain = candidate.Domain(direction);
	std::vector<ON_2dPointArray> remapped;
	bool valid = true;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     valid && data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); valid && segment !=
		    (*data)->segments->end(); ++segment) {
		if (!*segment) {
		    remapped.push_back(ON_2dPointArray());
		    continue;
		}
		ON_2dPointArray points(**segment);
		for (int point = 0; valid && point < points.Count(); ++point) {
		    const ON_2dPoint original = points[point];
		    double parameter = original[direction];
		    while (parameter < new_domain.Min() - endpoint_guard)
			parameter += period;
		    while (parameter > new_domain.Max() + endpoint_guard)
			parameter -= period;
		    points[point][direction] = parameter;
		    const ON_3dPoint old_lift = surface->PointAt(original.x, original.y);
		    const ON_3dPoint new_lift = candidate.PointAt(
			points[point].x, points[point].y);
		    valid = old_lift.IsValid() && new_lift.IsValid() &&
			old_lift.DistanceTo(new_lift) <= tolerance;
		    if (point > 0 && fabs(points[point][direction] -
			    points[point - 1][direction]) > 0.5 * period)
			valid = false;
		}
		remapped.push_back(points);
	    }
	}
	if (!valid)
	    continue;

	*nurbs = candidate;
	size_t remapped_index = 0;
	for (std::list<PBCData *>::iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		 ++segment, ++remapped_index) {
		if (*segment && remapped_index < remapped.size())
		    **segment = remapped[remapped_index];
	    }
	}
	return true;
    }
    return false;
}


static bool
snap_seam_pullback_pair(std::list<PBCData *> &pullbacks, PBCData *first, PBCData *second,
	ON_BrepLoop::TYPE expected_loop_type, double tolerance)
{
    if (!first || !second || first->surf != second->surf)
	return false;

    struct PairCandidate {
	int direction;
	double first_value;
	double second_value;
	double score;
	double area;
	double gap;
    };
    std::vector<PairCandidate> candidates;
    for (int direction = 0; direction < 2; ++direction) {
	if (!first->surf->IsClosed(direction))
	    continue;
	const ON_Interval domain = first->surf->Domain(direction);
	if (!domain.IsIncreasing())
	    continue;
	for (int reverse = 0; reverse < 2; ++reverse) {
	    const double first_value = reverse ? domain.Max() : domain.Min();
	    const double second_value = reverse ? domain.Min() : domain.Max();
	    double first_score = 0.0;
	    double second_score = 0.0;
	    if (!seam_boundary_score(first, direction, first_value, tolerance, &first_score) ||
		!seam_boundary_score(second, direction, second_value, tolerance, &second_score))
		continue;
	    double area = 0.0;
	    double gap = 0.0;
	    pullback_loop_metrics(pullbacks, first, first_value, second, second_value,
		direction, domain.Length(), &area, &gap);
	    candidates.push_back({direction, first_value, second_value,
		first_score + second_score, area, gap});
	}
    }
    if (candidates.empty())
	return false;

    PairCandidate *best = NULL;
    for (std::vector<PairCandidate>::iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate) {
	const bool expected_orientation =
	    (expected_loop_type == ON_BrepLoop::outer && candidate->area > 0.0) ||
	    (expected_loop_type == ON_BrepLoop::inner && candidate->area < 0.0);
	const bool best_orientation = best &&
	    ((expected_loop_type == ON_BrepLoop::outer && best->area > 0.0) ||
	     (expected_loop_type == ON_BrepLoop::inner && best->area < 0.0));
	if (!best || (expected_orientation && !best_orientation) ||
	    (expected_orientation == best_orientation && candidate->gap < best->gap) ||
	    (expected_orientation == best_orientation &&
	     fabs(candidate->gap - best->gap) <= ON_ZERO_TOLERANCE && candidate->score < best->score))
	    best = &*candidate;
    }
    if (!best)
	return false;

    bool changed = false;

    const double period = first->surf->Domain(best->direction).Length();
    ON_2dPoint previous;
    bool have_previous = false;
    for (std::list<PBCData *>::iterator data = pullbacks.begin(); data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	for (std::list<ON_2dPointArray *>::iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (!*segment)
		continue;
	    for (int i = 0; i < (*segment)->Count(); ++i) {
		ON_2dPoint *point = (*segment)->At(i);
		double value = (*point)[best->direction];
		if (*data == first)
		    value = best->first_value;
		else if (*data == second)
		    value = best->second_value;
		else if (have_previous)
		    value += round((previous[best->direction] - value) / period) * period;
		if (fabs((*point)[best->direction] - value) > ON_ZERO_TOLERANCE) {
		    (*point)[best->direction] = value;
		    changed = true;
		}
		previous = *point;
		have_previous = true;
	    }
        }
    }
    return changed;
}


static size_t
snap_pullback_loop_endpoints(std::list<PBCData *> &pullbacks, const ON_Brep *brep,
    double tolerance)
{
    struct SegmentRef {
	PBCData *data;
	ON_2dPointArray *samples;
    };
    std::vector<SegmentRef> segments;
    if (!brep || tolerance <= 0.0) return 0;
    for (std::list<PBCData *>::iterator data = pullbacks.begin();
	 data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments || !(*data)->surf || !(*data)->edge) continue;
	for (std::list<ON_2dPointArray *>::iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (*segment && (*segment)->Count() >= 2)
		segments.push_back({*data, *segment});
	}
    }
    if (segments.empty()) return 0;

    /* A repeated edge that has already been placed exactly on the two sides
     * of a closed surface is a proven seam bridge.  Its boundary coordinate
     * is authoritative: moving one of its endpoints to an equivalent,
     * off-boundary periodic image makes openNURBS classify the trim as a seam
     * with not_iso geometry. */
    const auto is_pinned_seam_endpoint = [&pullbacks](PBCData *candidate,
	const ON_2dPoint &endpoint) {
	if (!candidate || !candidate->edge || !candidate->surf || !candidate->segments)
	    return false;
	size_t edge_uses = 0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (*data && (*data)->edge == candidate->edge)
		++edge_uses;
	}
	if (edge_uses != 2)
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!candidate->surf->IsClosed(direction))
		continue;
	    const ON_Interval domain = candidate->surf->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		if (fabs(endpoint[direction] - boundary) > ON_ZERO_TOLERANCE)
		    continue;
		bool on_boundary = true;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			candidate->segments->begin();
		     on_boundary && segment != candidate->segments->end(); ++segment) {
		    if (!*segment) {
			on_boundary = false;
			break;
		    }
		    for (int point = 0; point < (*segment)->Count(); ++point) {
			if (fabs((**segment)[point][direction] - boundary) >
				ON_ZERO_TOLERANCE) {
			    on_boundary = false;
			    break;
			}
		    }
		}
		if (on_boundary)
		    return true;
	    }
	}
	return false;
    };

    size_t changed = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
	SegmentRef &current = segments[i];
	SegmentRef &next = segments[(i + 1) % segments.size()];
	ON_2dPoint *end = current.samples->At(current.samples->Count() - 1);
	ON_2dPoint *start = next.samples->At(0);
	if (!end || !start || end->DistanceTo(*start) <= ON_ZERO_TOLERANCE ||
	    current.data->surf != next.data->surf)
	    continue;

	int shared_vertex = -1;
	if (current.data != next.data) {
	    for (int current_vertex = 0; current_vertex < 2; ++current_vertex) {
		for (int next_vertex = 0; next_vertex < 2; ++next_vertex) {
		    if (current.data->edge->m_vi[current_vertex] ==
			next.data->edge->m_vi[next_vertex]) {
			if (shared_vertex >= 0 && shared_vertex !=
				current.data->edge->m_vi[current_vertex]) {
			    shared_vertex = -2;
			    break;
			}
			shared_vertex = current.data->edge->m_vi[current_vertex];
		    }
		}
		if (shared_vertex == -2) break;
	    }
	    if (shared_vertex < 0 || shared_vertex >= brep->m_V.Count())
		continue;
	}

	const ON_3dPoint lifted_end = current.data->surf->PointAt(end->x, end->y);
	const ON_3dPoint lifted_start = next.data->surf->PointAt(start->x, start->y);
	if (!lifted_end.IsValid() || !lifted_start.IsValid())
	    continue;
	ON_2dPoint common = *end;
	const bool end_is_pinned_seam = is_pinned_seam_endpoint(current.data, *end);
	const bool start_is_pinned_seam = is_pinned_seam_endpoint(next.data, *start);
	if (end_is_pinned_seam != start_is_pinned_seam) {
	    common = end_is_pinned_seam ? *end : *start;
	    if (shared_vertex >= 0 && common.IsValid()) {
		const ON_3dPoint vertex = brep->m_V[shared_vertex].point;
		const ON_3dPoint lifted_common = current.data->surf->PointAt(common.x, common.y);
		if (!lifted_common.IsValid() || lifted_common.DistanceTo(vertex) > tolerance) {
		    const ON_2dPoint alternate = end_is_pinned_seam ? *start : *end;
		    const ON_3dPoint lifted_alternate = current.data->surf->PointAt(
			alternate.x, alternate.y);
		    bool preserves_boundary = lifted_alternate.IsValid() &&
			lifted_alternate.DistanceTo(vertex) <= tolerance;
		    const ON_2dPoint pinned = end_is_pinned_seam ? *end : *start;
		    PBCData *pinned_data = end_is_pinned_seam ? current.data : next.data;
		    for (int direction = 0; direction < 2 && preserves_boundary; ++direction) {
			if (!current.data->surf->IsClosed(direction))
			    continue;
			const ON_Interval domain = current.data->surf->Domain(direction);
			for (int side = 0; side < 2; ++side) {
			    if (fabs(pinned[direction] - domain[side]) > ON_ZERO_TOLERANCE)
				continue;
			    bool curve_on_boundary = true;
			    for (std::list<ON_2dPointArray *>::const_iterator segment =
				    pinned_data->segments->begin(); curve_on_boundary &&
				    segment != pinned_data->segments->end(); ++segment) {
				if (!*segment) {
				    curve_on_boundary = false;
				    break;
				}
				for (int point = 0; point < (*segment)->Count(); ++point) {
				    if (fabs((**segment)[point][direction] - domain[side]) >
					    ON_ZERO_TOLERANCE) {
					curve_on_boundary = false;
					break;
				    }
				}
			    }
			    if (curve_on_boundary && fabs(alternate[direction] -
				    domain[side]) > ON_ZERO_TOLERANCE) {
				preserves_boundary = false;
				break;
			    }
			}
		    }
		    if (!preserves_boundary)
			continue;
		    common = alternate;
		}
	    }
	} else if (shared_vertex >= 0) {
	    const ON_3dPoint vertex = brep->m_V[shared_vertex].point;
	    const bool end_valid = lifted_end.DistanceTo(vertex) <= tolerance;
	    const bool start_valid = lifted_start.DistanceTo(vertex) <= tolerance;
	    if (!end_valid || !start_valid) {
		if (end_valid) {
		    common = *end;
		} else if (start_valid) {
		    common = *start;
		} else {
		    if (!current.data->context)
			current.data->context = std::make_shared<brlcad::PullbackContext>();
		    ON_3dPoint projected;
		    double distance = DBL_MAX;
		    if (!current.data->context->SurfaceClosestPoint(current.data->surf,
			    vertex, common, projected, distance, 0, tolerance, tolerance) ||
			distance > tolerance)
			continue;
		    /* Select the periodic image nearest the existing loop endpoint. */
		    for (int direction = 0; direction < 2; ++direction) {
			if (!current.data->surf->IsClosed(direction)) continue;
			const double period = current.data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    common[direction] += round(((*end)[direction] - common[direction]) /
				period) * period;
		    }
		}
	    }
	} else if (lifted_end.DistanceTo(lifted_start) > tolerance) {
	    continue;
	}

	/* The two UVs are proven to represent the same topological 3-D point
	 * within the model uncertainty.  Use the preceding trim's endpoint so
	 * openNURBS sees an exactly closed parameter-space loop. */
	*end = common;
	*start = common;
	++changed;
    }
    return changed;
}


static ON_Curve *
closed_edge_iso_line(PBCData *data, const ON_2dPointArray &samples, double tolerance)
{
    if (!data || !data->edge || !data->surf || !data->curve || samples.Count() < 2 ||
	data->edge->m_vi[0] != data->edge->m_vi[1] || tolerance <= 0.0)
	return NULL;

    struct IsoCandidate {
	int direction;
	double value;
	double score;
	ON_2dPoint start;
	ON_2dPoint end;
    };
    std::vector<IsoCandidate> candidates;
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval domain = data->surf->Domain(direction);
	if (!domain.IsIncreasing())
	    continue;
	for (int side = 0; side < 2; ++side) {
	    const double value = domain[side];
	    ON_2dPoint start = samples[0];
	    ON_2dPoint end = samples[samples.Count() - 1];
	    start[direction] = value;
	    end[direction] = value;
	    if (fabs(end[1 - direction] - start[1 - direction]) <= ON_ZERO_TOLERANCE)
		continue;

	    bool forward_valid = true;
	    bool reverse_valid = true;
	    const ON_Interval curve_domain = data->curve->Domain();
	    for (int i = 0; i <= 64; ++i) {
		const double parameter = static_cast<double>(i) / 64.0;
		const ON_2dPoint uv = (1.0 - parameter) * start + parameter * end;
		const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
		if (!lifted.IsValid()) {
		    forward_valid = false;
		    reverse_valid = false;
		    break;
		}
		if (lifted.DistanceTo(data->curve->PointAt(curve_domain.ParameterAt(parameter))) > tolerance)
		    forward_valid = false;
		if (lifted.DistanceTo(data->curve->PointAt(curve_domain.ParameterAt(1.0 - parameter))) > tolerance)
		    reverse_valid = false;
	    }
	    if (!forward_valid && !reverse_valid)
		continue;

	    double score = 0.0;
	    for (int i = 0; i < samples.Count(); ++i)
		score += fabs(samples[i][direction] - value) / domain.Length();
	    candidates.push_back({direction, value, score, start, end});
	}
    }

    IsoCandidate *best = NULL;
    for (std::vector<IsoCandidate>::iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate) {
	if (!best || candidate->score < best->score)
	    best = &*candidate;
    }
    return best ? new ON_LineCurve(best->start, best->end) : NULL;
}


/* A plane has an exact affine inverse, so do not send planar edge curves
 * through the iterative closest-point pullback.  Besides being faster, this
 * handles high-degree closed and half-closed curves whose coincident end
 * points can make an unseeded numerical search ambiguous.  The lift check is
 * deliberately retained: if an exporter supplied inconsistent topology, the
 * caller falls back to the bounded general pullback instead of accepting it. */
static PBCData *
exact_planar_pullback(const ON_Surface *surface, const ON_Curve *curve,
    double tolerance, ON_Curve **exact_curve)
{
    if (exact_curve) *exact_curve = NULL;
    const ON_PlaneSurface *plane_surface = ON_PlaneSurface::Cast(surface);
    if (!plane_surface || !curve || !exact_curve || tolerance <= 0.0)
	return NULL;

    const ON_Plane &plane = plane_surface->m_plane;
    ON_Xform world_to_plane(ON_Xform::IdentityTransformation);
    if (!world_to_plane.ChangeBasis(ON_Plane::World_xy, plane))
	return NULL;

    ON_Curve *pcurve = curve->DuplicateCurve();
    if (!pcurve || !pcurve->Transform(world_to_plane)) {
	delete pcurve;
	return NULL;
    }

    ON_Xform plane_to_parameter(ON_Xform::IdentityTransformation);
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval extents = plane_surface->Extents(direction);
	const ON_Interval domain = plane_surface->Domain(direction);
	if (!extents.IsIncreasing() || !domain.IsIncreasing()) {
	    delete pcurve;
	    return NULL;
	}
	const double scale = domain.Length() / extents.Length();
	plane_to_parameter.m_xform[direction][direction] = scale;
	plane_to_parameter.m_xform[direction][3] = domain.Min() - scale * extents.Min();
    }
    if (!pcurve->Transform(plane_to_parameter) || !pcurve->ChangeDimension(2)) {
	delete pcurve;
	return NULL;
    }

    const ON_Interval curve_domain = curve->Domain();
    const ON_Interval pcurve_domain = pcurve->Domain();
    bool forward = true;
    bool reverse = true;
    for (int sample = 0; sample <= 64; ++sample) {
	const double normalized = static_cast<double>(sample) / 64.0;
	const ON_3dPoint point = curve->PointAt(curve_domain.ParameterAt(normalized));
	const ON_3dPoint forward_uv = pcurve->PointAt(pcurve_domain.ParameterAt(normalized));
	const ON_3dPoint reverse_uv = pcurve->PointAt(pcurve_domain.ParameterAt(1.0 - normalized));
	forward = forward && point.DistanceTo(surface->PointAt(forward_uv.x, forward_uv.y)) <= tolerance;
	reverse = reverse && point.DistanceTo(surface->PointAt(reverse_uv.x, reverse_uv.y)) <= tolerance;
    }
    if (!forward && !reverse) {
	delete pcurve;
	return NULL;
    }
    if (!forward && !pcurve->Reverse()) {
	delete pcurve;
	return NULL;
    }

    ON_2dPointArray *samples = new ON_2dPointArray();
    const ON_Interval validated_domain = pcurve->Domain();
    for (int sample = 0; sample <= 64; ++sample) {
	const double normalized = static_cast<double>(sample) / 64.0;
	const ON_3dPoint uv = pcurve->PointAt(validated_domain.ParameterAt(normalized));
	samples->Append(ON_2dPoint(uv.x, uv.y));
    }

    PBCData *data = new PBCData();
    data->tolerance = tolerance;
    data->flatness = tolerance;
    data->curve = curve;
    data->surf = surface;
    data->surftree = NULL;
    data->segments = new std::list<ON_2dPointArray *>();
    data->segments->push_back(samples);
    data->edge = NULL;
    data->order_reversed = false;
    *exact_curve = pcurve;
    return data;
}

ON_Brep *
AdvancedBrepShapeRepresentation::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
AdvancedBrepShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    LIST_OF_REPRESENTATION_ITEMS::iterator i;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    for (i = items.begin(); i != items.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }

    return true;
}

ON_Brep *
ShellBasedSurfaceModel::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
ShellBasedSurfaceModel::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    LIST_OF_SHELL_BOUNDARIES::iterator i;
    for (i = sbsm_boundary.begin(); i != sbsm_boundary.end(); ++i) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    return true;
}

//
// Curve handlers
//
bool
BezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }
    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	// knot index (>= 0 and < Order + CV_count - 2)
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    V_MIN(multiplicity, degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
UniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


//
// Surface handlers
//
bool
BezierSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add bezier surface
    //ON_BezierSurface* surf = ON_BezierSurface::New(3, false, u_degree+1, v_degree+1);
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
BSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
	    v++;
	}
	u++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}

ON_Brep *
BSplineSurfaceWithKnots::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    ON_Brep *b2 = ON_Brep::New();
    b2->NewFace(*brep->m_S[0]);
    b2->Flip();

    delete brep;

    return b2;
}


bool
BSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    if (u_closed == 1) {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < u_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == this->u_multiplicities.end() && (multiplicity < u_degree) && (knot_value > 1.0)) {
		break;
	    }

	    V_MIN(multiplicity, u_degree);

	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);

	    V_MIN(multiplicity, u_degree);

	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    if (v_closed == 1) {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < v_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == v_multiplicities.end() && (multiplicity < v_degree) && (knot_value > 1.0)) {
		break;
	    }

	    V_MIN(multiplicity, v_degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);

	    V_MIN(multiplicity, v_degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
	    v++;
	}
	u++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}


bool
QuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add rational bezier surface
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
RationalBSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    LIST_OF_REALS::iterator r;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
	}
	u++;
	w++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}


bool
RationalBSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (self_intersect) {
    }
    if (u_closed) {
    }
    if (v_closed) {
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, true, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);

	V_MIN(multiplicity, u_degree);

	for (int j = 0; j < multiplicity; j++, knot_index++) {
	    surf->SetKnot(0, knot_index, knot_value);
	}
	r++;
	m++;
    }
    m = v_multiplicities.begin();
    r = v_knots.begin();
    knot_index = 0;
    while (m != v_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);

	V_MIN(multiplicity, v_degree);

	for (int j = 0; j < multiplicity; j++) {
	    surf->SetKnot(1, knot_index++, knot_value);
	}
	r++;
	m++;
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
	}
	u++;
	w++;
    }

    ON_id = brep->AddSurface(surf);

    return true;
}


bool
RationalQuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
UniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


void
FaceSurface::AddFace(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    ON_BrepFace &face = brep->NewFace(face_geometry->GetONId());
    if (same_sense == BTrue) {
	face.m_bRev = false;
    } else {
	face.m_bRev = true;
	face.Reverse(0); //need to remove here but check for reversed face in raytracer
    }

    ON_id = face.m_face_index;
}


bool
FaceSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (step)
	step->SetProgress("computing exact STEP face edge bounds", 0, 0, id);

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    ON_BoundingBox *bb = GetEdgeBounds(brep);
    if (!bb) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error calculating openNURBS brep bounds." << std::endl;
	return false;
    }
	if (brlcad::PullbackWorkCancelled()) {
	    delete bb;
	    return false;
	}

    face_geometry->SetCurveBounds(bb);
    delete bb;

    if (step)
	step->SetProgress("building exact STEP face surface", 0, 0, id);
    if (!face_geometry->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    AddFace(brep);
	if (brlcad::PullbackWorkCancelled()) return false;

    //TODO: remove debugging code
    if ((false) && (ON_id == 72)) {
	std::cerr << "Debug:LoadONBrep for FaceSurface:" << ON_id << std::endl;
    }
    if (step)
	step->SetProgress("constructing exact STEP face loops", 0, 0, id);
    if (!Face::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    if (reverse) {
	ON_BrepFace *face = brep->Face(ON_id);
	face->Reverse(1);
	face->m_bRev = face->m_bRev ? false : true;
    }
    return true;
}


bool
OrientedFace::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    if (!face_element->LoadONBrep(brep)) {
#ifndef AP242
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
#endif
	return false;
    }

    return true;
}


void
Point::AddVertex(ON_Brep *UNUSED(brep))
{
    std::cerr << "Warning: " << entityname << "::AddVertex() should be overridden by parent class." << std::endl;
}


void
CartesianPoint::AddVertex(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (vertex_index < 0) {
	ON_3dPoint p(coordinates[0] * LocalUnits::length, coordinates[1] * LocalUnits::length, coordinates[2] * LocalUnits::length);
	ON_BrepVertex &v = brep->NewVertex(p);
	vertex_index = v.m_vertex_index;
	v.m_tolerance = LocalUnits::tolerance;
	ON_id = v.m_vertex_index;
    }
}


void
BSplineCurve::AddPolyLine(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (ON_id < 0) {
	int num_control_points = control_points_list.size();

	if ((degree == 1) && (num_control_points >= 2)) {
	    LIST_OF_POINTS::iterator i = control_points_list.begin();
	    CartesianPoint *cp = (*i);
	    while ((++i) != control_points_list.end()) {
		ON_3dPoint start_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		cp = (*i);
		ON_3dPoint end_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		ON_LineCurve *line = new ON_LineCurve(ON_3dPoint(start_point), ON_3dPoint(end_point));
		brep->m_C3.Append(line);
	    }
	} else if (num_control_points > 2) {
	    ON_NurbsCurve *c = ON_NurbsCurve::New(3, false, degree + 1, num_control_points);
	    /* FIXME: do something with c */
	    delete c;
	} else {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading polyLine." << std::endl;
	}
    }
}


void
VertexPoint::AddVertex(ON_Brep *brep)
{
    vertex_geometry->AddVertex(brep);
}


ON_BoundingBox *
Face::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return NULL;
	ON_BoundingBox *bb = (*i)->GetEdgeBounds(brep);
	if (bb != NULL) {
	    if (u == NULL) {
		u = new ON_BoundingBox();
	    }
	    u->Union(*bb);
	}
	delete bb;
    }
    return u;
}


bool
Face::LoadONBrep(ON_Brep *brep)
{
    //TODO: Check for Outer bound if none check for
    // direction perhaps offer input option possibly
    // check for outer spanning to bounds
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (step)
	    step->SetProgress("constructing exact STEP face loop", 0, 0,
		(*i) ? (*i)->STEPid() : id, static_cast<uint64_t>(id), "face");
	(*i)->SetFaceIndex(ON_id);
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    return true;
}


bool
FaceOuterBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    SetOuter();

    if (!FaceBound::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


ON_BoundingBox *
FaceBound::GetEdgeBounds(ON_Brep *brep)
{
    return bound->GetEdgeBounds(brep);
}


bool
FaceBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id < 0) {
	enum ON_BrepLoop::TYPE btype;
	if (IsInner()) {
	    btype = ON_BrepLoop::inner;
	} else {
	    btype = ON_BrepLoop::outer;
	}
	ON_BrepLoop &loop = brep->NewLoop(btype, brep->m_F[ON_face_index]);
	ON_id = loop.m_loop_index;
    }
    bound->SetLoopIndex(ON_id);
    if (!bound->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!Oriented()) {
	ON_BrepLoop &loop = brep->m_L[ON_id];
	brep->FlipLoop(loop);
    }
    /* lets use the cues from STEP (Oriented) for now but leave this here
     * in case we have to go back to brute force

     if (IsInner()) {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) > 0) {
     brep->FlipLoop(loop);
     }
     } else {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) < 0) {
     brep->FlipLoop(loop);
     }
     }
    */

    return true;
}

bool
EdgeCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    if ((false) && (brep->m_E.Count() == 484)) {
	std::cerr << "Debug:LoadONBrep for EdgeCurve:" << brep->m_E.Count() << std::endl;
    }

    Vertex *start = NULL;
    Vertex *end = NULL;
    if (same_sense == 1) {
	start = edge_start;
	end = edge_end;
    } else {
	start = edge_end;
	end = edge_start;
    }
    edge_geometry->Start(start);
    edge_geometry->End(end);

    if (!edge_geometry->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_BrepEdge &edge = brep->NewEdge(brep->m_V[start->GetONId()], brep->m_V[end->GetONId()], edge_geometry->GetONId());
    edge.m_tolerance = LocalUnits::tolerance;
    edge.m_edge_user.i = id;
    ON_id = edge.m_edge_index;
    if (same_sense != 1) {
	edge.Reverse();
    }

    return true;
}

bool
OrientedEdge::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    //already loaded
    }

    //TODO: remove debugging code
    //if ((false) && (brep->m_E.Count() == 5)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << brep->m_E.Count() << std::endl;
    //}

    if (!edge_start->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_end->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_element->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_id = edge_element->GetONId();

    //TODO: remove debugging code
    //if ((true) && (ON_id == 12)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << ON_id << std::endl;
    //}
    return true;
}


ON_BoundingBox *
Path::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;

    LIST_OF_ORIENTED_EDGES::iterator i;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) {
	    delete u;
	    return NULL;
	}
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    delete u;
	    return NULL;
	}
	if (u == NULL) {
	    u = new ON_BoundingBox();
	}
	const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	const ON_Curve *curve = edge->EdgeCurveOf();
	u->Union(curve->BoundingBox());
    }

    return u;
}


bool
Path::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;
    LIST_OF_ORIENTED_EDGES::iterator i;

    if ((false) && (id == 29429)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }

    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    //TODO: remove debugging code
    if ((false) && (id == 26089)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }
    if (!LoadONTrimmingCurves(brep)) {
	return false;
    }

    return true;
}


bool
Path::ShiftSurfaceSeam(ON_Brep *brep, double *t)
{
    const ON_BrepLoop *loop = NULL;
    const ON_BrepFace *face = NULL;
    const ON_Surface *surface = NULL;
    double ang_min = 0.0;
    double smin, smax;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    loop = &brep->m_L[ON_path_index];
    if (!loop) {
	/* nothing to do */
	return false;
    }

    face = loop->Face();
    if (!face) {
	/* nothing to do */
	return false;
    }

    surface = face->SurfaceOf();
    if (!surface) {
	/* nothing to do */
	return false;
    }

    if (surface->IsCone() || surface->IsCylinder()) {
	if (surface->IsClosed(0)) {
	    surface->GetDomain(0, &smin, &smax);
	} else {
	    surface->GetDomain(1, &smin, &smax);
	}

	LIST_OF_ORIENTED_EDGES::iterator i;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();
	    double tmin, tmax;
	    curve->GetDomain(&tmin, &tmax);

	    if (((tmin < 0.0) && (tmax > 0.0)) && ((tmin > smin) || (tmax < smax))) {
		V_MIN(ang_min, tmin);
	    }

	}

	if (ang_min < 0.0) {
	    *t = ang_min;
	    return true;
	}
    }
    return false;
}


#ifdef _DEBUG_TESTING_
bool _debug_print_ = false;
#endif

bool
Path::LoadONTrimmingCurves(ON_Brep *brep)
{
    LIST_OF_ORIENTED_EDGES::iterator i;
    list<PBCData *> curve_pullback_samples;
    std::map<PBCData *, ON_Curve *> exact_pullbacks;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_BrepLoop *loop = &brep->m_L[ON_path_index];
    loop->m_loop_user.i = id;
    ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face->SurfaceOf();
    const int initial_trim_count = loop->TrimCount();

#if 0
    if (surface) {
	double surface_width, surface_height;
	if (surface->GetSurfaceSize(&surface_width, &surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }
#endif
#ifdef _DEBUG_TESTING_
    if (_debug_print_) {
	int curve_cnt = 0;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();

	    ON_Interval interval = curve->Domain();
	    double delta = interval.Length()/100.0;
	    for(int j =0; j < 100; j++) {
		ON_3dPoint p = curve->PointAt(interval.m_t[0] + j*delta);
		std::cerr << "in pt_" << curve_cnt << " sph " << p.x << " " << p.y << " " << p.z << " 0.1000"  << std::endl;
		curve_cnt++;
	    }
	}
    }
#endif
    // build surface tree making sure not to remove trimmed subsurfaces
    // since currently building trims and need full tree
    // bool removeTrimmed = false;

    //TODO: remove debugging code
    if ((false) && (id == 24894)) {
	std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
    }
    PBCData *data = NULL;
    LIST_OF_ORIENTED_EDGES::iterator prev, next;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) {
	    for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
		 exact != exact_pullbacks.end(); ++exact)
		delete exact->second;
	    while (!curve_pullback_samples.empty()) {
		destroy_pullback_data(curve_pullback_samples.front());
		curve_pullback_samples.pop_front();
	    }
	    return false;
	}
	if (step)
	    step->SetProgress("constructing exact STEP trim pullback", 0, 0,
		(*i) ? (*i)->STEPid() : id, static_cast<uint64_t>(id), "loop");
	// grab the curve for this edge, face and surface
	const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	const ON_Curve *curve = edge->EdgeCurveOf();
	bool orientWithCurve = (*i)->OrientWithEdge();

	if ((false) && (id == 34193)) {
	    std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
	}
	ON_Curve *exact_curve = NULL;
	data = exact_planar_pullback(surface, curve, LocalUnits::tolerance, &exact_curve);
	if (data)
	    exact_pullbacks[data] = exact_curve;
	else {
	    if (step && step->Verbose() && ON_PlaneSurface::Cast(surface))
		std::cerr << "EDGE_LOOP #" << id << ": exact planar trim rejected for edge #"
		    << (*i)->STEPid() << "; using bounded pullback" << std::endl;
	    data = pullback_samples(surface, curve, LocalUnits::tolerance,
		LocalUnits::tolerance, LocalUnits::tolerance, LocalUnits::tolerance);
	    if (data && pullback_sample_count(data) < 2 &&
		    !surface->IsClosed(0) && !surface->IsClosed(1)) {
		const double resolution = short_curve_pullback_resolution(curve,
		    LocalUnits::tolerance);
		PBCData *refined = pullback_samples(surface, curve,
		    LocalUnits::tolerance, LocalUnits::tolerance,
		    resolution, resolution);
		if (refined && pullback_sample_count(refined) >= 2) {
		    destroy_pullback_data(data);
		    data = refined;
		    if (step && step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "refined pullback resolution for a feature below model tolerance");
		} else {
		    destroy_pullback_data(refined);
		}
	    }
	}
	if (data == NULL) {
	    if (step && step->Verbose())
		std::cerr << "EDGE_LOOP #" << id << ": could not construct a validated trim for edge #"
		    << (*i)->STEPid() << std::endl;
	    continue;
	}

	if (!orientWithCurve) {
	    data->order_reversed = true;
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && exact->second && !exact->second->Reverse()) {
		delete exact->second;
		exact_pullbacks.erase(exact);
	    }
	} else {
	    data->order_reversed = false;
	}
	data->edge = edge;
	curve_pullback_samples.push_back(data);
	if (!orientWithCurve) {
	    list<ON_2dPointArray *>::iterator si;
	    si = data->segments->begin();
	    list<ON_2dPointArray *> rsegs;
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		samples->Reverse();
		rsegs.push_front(samples);
		si++;
	    }
	    data->segments->clear();
	    si = rsegs.begin();
	    while (si != rsegs.end()) {
		ON_2dPointArray *samples = (*si);
		data->segments->push_back(samples);
		si++;
	    }
	    rsegs.clear();
	}
    }
    if (curve_pullback_samples.size() != edge_list.size()) {
	for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	     exact != exact_pullbacks.end(); ++exact)
	    delete exact->second;
	while (!curve_pullback_samples.empty()) {
	    data = curve_pullback_samples.front();
	    while (data->segments && !data->segments->empty()) {
		delete data->segments->front();
		data->segments->pop_front();
	    }
	    delete data->segments;
	    delete data;
	    curve_pullback_samples.pop_front();
	}
	return false;
    }
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("Before check_pullback_data", curve_pullback_samples, false);
    }
#endif
    // Exact affine planar pullbacks cannot cross a parametric seam or end at
    // a surface singularity.  Preserve those curves verbatim; the general
    // seam resolver is intentionally limited to periodic/non-planar results.
    const bool exact_open_surface = exact_pullbacks.size() == curve_pullback_samples.size() &&
	!surface->IsClosed(0) && !surface->IsClosed(1);
    // check for seams and singularities
    if (!exact_open_surface && !check_pullback_data(curve_pullback_samples)) {
	std::cerr << "Error: Can not resolve seam or singularity issues." << std::endl;
    }

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	if (align_nurbs_surface_seam(curve_pullback_samples, surface,
		LocalUnits::tolerance)) {
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"aligned a periodic NURBS surface seam with its closed edge");
	} else if (relocate_nurbs_surface_seam_away_from_boundary(
		curve_pullback_samples, surface, LocalUnits::tolerance)) {
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"relocated a closed NURBS surface seam outside the face boundary");
	}
    }

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->edge)
		continue;
	    size_t edge_uses = 0;
	    PBCData *paired_data = NULL;
	    for (std::list<PBCData *>::const_iterator other = curve_pullback_samples.begin();
		 other != curve_pullback_samples.end(); ++other) {
		if (*other && (*other)->edge == current_data->edge) {
		    ++edge_uses;
		    if (*other != current_data && paired_data == NULL)
			paired_data = *other;
		}
	    }
	    /* Process the pair only from its first occurrence in loop order. */
	    bool current_is_first = true;
	    for (std::list<PBCData *>::const_iterator prior = curve_pullback_samples.begin();
		 prior != current && prior != curve_pullback_samples.end(); ++prior) {
		if (*prior && (*prior)->edge == current_data->edge) {
		    current_is_first = false;
		    break;
		}
	    }
	if (edge_uses == 2 && current_is_first && snap_seam_pullback_pair(
		curve_pullback_samples, current_data, paired_data, loop->m_type, LocalUnits::tolerance)) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "normalized seam pcurve within model tolerance");
	    }
	}
	/* Normalize proven closed boundary edges before closing adjacent samples.
	 * Doing this during trim emission is too late: a boundary line can move an
	 * endpoint after its neighboring seam curve has already been constructed. */
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::iterator segment = current_data->segments->begin();
		 segment != current_data->segments->end(); ++segment) {
		if (!*segment || (*segment)->Count() < 2)
		    continue;
		ON_Curve *boundary = closed_edge_iso_line(current_data, **segment,
		    LocalUnits::tolerance);
		if (!boundary)
		    continue;
		const ON_3dPoint start = boundary->PointAtStart();
		const ON_3dPoint end = boundary->PointAtEnd();
		(*segment)->At(0)->Set(start.x, start.y);
		(*segment)->At((*segment)->Count() - 1)->Set(end.x, end.y);
		delete boundary;
	    }
	}
	const size_t endpoint_repairs = snap_pullback_loop_endpoints(
	    curve_pullback_samples, brep, LocalUnits::tolerance);
	if (endpoint_repairs) {
	    for (size_t repair = 0; repair < endpoint_repairs; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "snapped adjacent pcurve endpoint within model tolerance");
	}
	if (endpoint_repairs) {
	    bool seam_changed = false;
	    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
		 current != curve_pullback_samples.end(); ++current) {
		if (brlcad::PullbackWorkCancelled()) break;
		PBCData *current_data = *current;
		if (!current_data || !current_data->edge)
		    continue;
		PBCData *paired_data = NULL;
		size_t edge_uses = 0;
		bool current_is_first = true;
		bool reached_current = false;
		for (std::list<PBCData *>::iterator other = curve_pullback_samples.begin();
		     other != curve_pullback_samples.end(); ++other) {
		    if (other == current) reached_current = true;
		    if (*other && (*other)->edge == current_data->edge) {
			++edge_uses;
			if (other != current && !paired_data) paired_data = *other;
			if (other != current && !reached_current) current_is_first = false;
		    }
		}
		if (edge_uses == 2 && current_is_first && snap_seam_pullback_pair(
			curve_pullback_samples, current_data, paired_data, loop->m_type,
			LocalUnits::tolerance)) {
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"normalized seam pcurve within model tolerance");
		    seam_changed = true;
		}
	    }
	    if (seam_changed) {
		const size_t final_endpoint_repairs = snap_pullback_loop_endpoints(
		    curve_pullback_samples, brep, LocalUnits::tolerance);
		for (size_t repair = 0; repair < final_endpoint_repairs; ++repair)
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"snapped adjacent pcurve endpoint within model tolerance");
	    }
	}
    }
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("After check_pullback_data", curve_pullback_samples, false);
    }
#endif
    list<PBCData *>::iterator cs = curve_pullback_samples.begin();
    list<PBCData *>::iterator next_cs;
    bool trim_construction_failed = false;

    cs = curve_pullback_samples.begin();
    while (cs != curve_pullback_samples.end()) {
	if (brlcad::PullbackWorkCancelled()) {
	    trim_construction_failed = true;
	    break;
	}
	next_cs = cs;
	next_cs++;
	if (next_cs == curve_pullback_samples.end()) {
	    next_cs = curve_pullback_samples.begin();
	}
	data = (*cs);
	list<ON_2dPointArray *>::iterator si;
	si = data->segments->begin();
	PBCData *ndata = (*next_cs);
	list<ON_2dPointArray *>::iterator nsi;
	nsi = ndata->segments->begin();
	ON_2dPointArray *nsamples = NULL;

	while (si != data->segments->end()) {
	    if (brlcad::PullbackWorkCancelled()) {
		trim_construction_failed = true;
		break;
	    }
	    nsi = si;
	    nsi++;
	    if (nsi == data->segments->end()) {
		PBCData *nsidata = (*next_cs);
		nsi = nsidata->segments->begin();
	    }
	    ON_2dPointArray *samples = (*si);
	    nsamples = (*nsi);

	    int trimCurve = brep->m_C2.Count();
	    //TODO: remove debugging code
	    if ((false) && (trimCurve == 68)) {
		std::cerr << "Debug:LoadONTrimmingCurves for Path:" << trimCurve << std::endl;
	    }
	    ON_Curve *c2d = NULL;
	    /* A short exact boundary can collapse to one pullback sample when its
	     * length is below the asserted model uncertainty.  Do not silently
	     * omit that topology edge.  Leave c2d unset so the bounded adjacent-
	     * endpoint reconstruction below can prove and restore the pcurve. */
	    if (samples && samples->Count() >= 2) {
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && data->segments->size() == 1 &&
		exact->second) {
		const ON_Interval exact_domain = exact->second->Domain();
	const ON_3dPoint exact_start = exact->second->PointAt(exact_domain.Min());
	const ON_3dPoint exact_end = exact->second->PointAt(exact_domain.Max());
	if (ON_2dPoint(exact_start.x, exact_start.y).DistanceTo((*samples)[0]) <=
			ON_ZERO_TOLERANCE &&
		    ON_2dPoint(exact_end.x, exact_end.y).DistanceTo(
			(*samples)[samples->Count() - 1]) <= ON_ZERO_TOLERANCE) {
		    c2d = exact->second;
		    exact->second = NULL;
		}
	    }
	    bool regenerated = false;
	    bool used_polyline_fallback = false;
	    if (!c2d && step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
		c2d = closed_edge_iso_line(data, *samples, LocalUnits::tolerance);
		if (c2d && (c2d->PointAtStart().DistanceTo((*samples)[0]) >
			ON_ZERO_TOLERANCE ||
		    c2d->PointAtEnd().DistanceTo((*samples)[samples->Count() - 1]) >
			ON_ZERO_TOLERANCE)) {
		    delete c2d;
		    c2d = NULL;
		}
		regenerated = c2d != NULL;
	    }
	    if (regenerated) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "regenerated closed-edge boundary pcurve within model tolerance");
	    } else if (!c2d) {
		const bool closed_edge = data->edge &&
		    data->edge->m_vi[0] == data->edge->m_vi[1];
		if (closed_edge) {
		    /* The local cubic fitter extrapolates open-curve endpoint
		     * tangents.  On a closed 3-D edge whose UV endpoints lie on
		     * different sides of a periodic domain, that fit can overshoot
		     * the already validated pullback samples and reverse one endpoint
		     * tangent.  Preserve the adaptive, lift-bounded UV path directly. */
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample)
			points.Append(ON_3dPoint((*samples)[sample].x,
			    (*samples)[sample].y, 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(points);
		    if (polyline->ChangeDimension(2) && polyline->IsValid())
			c2d = polyline;
		    else
			delete polyline;
		} else {
		    ON_2dPointArray interpolation_samples(*samples);
		    c2d = interpolateCurve(interpolation_samples);
		}
		if (!c2d) {
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample) {
			const ON_3dPoint point((*samples)[sample].x,
			    (*samples)[sample].y, 0.0);
			if (points.Count() > 0 && point.DistanceTo(
				points[points.Count() - 1]) <= ON_ZERO_TOLERANCE) {
			    points[points.Count() - 1] = point;
			    continue;
			}
			points.Append(point);
		    }
		    ON_PolylineCurve *polyline = points.Count() >= 2 ?
			new ON_PolylineCurve(points) : NULL;
		    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid()) {
			c2d = polyline;
			used_polyline_fallback = true;
		    } else {
			delete polyline;
		    }
		}
	    }
	    if (used_polyline_fallback && step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "preserved validated pullback samples after curve fitting failed");
	    }
	    if (!c2d && samples && samples->Count() < 2 && step &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		data->edge && data->curve) {
		/* The second use of a collapsed seam is the same exact STEP edge in
		 * reverse.  Reuse the first validated pcurve instead of asking a
		 * closest-point solver to distinguish endpoints whose separation is
		 * smaller than the model uncertainty. */
		for (int lti = 0; lti < loop->TrimCount() && !c2d; ++lti) {
		    const ON_BrepTrim *paired_trim = loop->Trim(lti);
		    if (!paired_trim || paired_trim->m_ei != data->edge->m_edge_index ||
			!paired_trim->TrimCurveOf())
			continue;
		    ON_Curve *candidate = paired_trim->DuplicateCurve();
		    if (!candidate)
			continue;
		    if (paired_trim->m_bRev3d != data->order_reversed &&
			!candidate->Reverse()) {
			delete candidate;
			continue;
		    }
		    const ON_Interval candidate_domain = candidate->Domain();
		    const ON_Interval edge_domain = data->curve->Domain();
		    bool candidate_valid = candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(data->order_reversed ?
				1.0 - fraction : fraction));
			if (!lifted.IsValid() || !target.IsValid() ||
				lifted.DistanceTo(target) > LocalUnits::tolerance)
			    candidate_valid = false;
		    }
		    if (!candidate_valid) {
			delete candidate;
			continue;
		    }
		    c2d = candidate;
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"reused an exact paired collapsed seam pcurve");
		}
	    }
	    if (!c2d && step && step->ImportOptions().repair ==
		    brlcad::step::RepairMode::Safe && loop->TrimCount() >
		    initial_trim_count && nsamples && nsamples->Count() > 0 &&
		    data->curve && data->surf) {
		const ON_BrepTrim *previous_trim = loop->Trim(loop->TrimCount() - 1);
		ON_3dPoint start = previous_trim ? previous_trim->PointAtEnd() :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint adjacent_start = start;
		ON_3dPoint end((*nsamples)[0].x, (*nsamples)[0].y, 0.0);
		const ON_Interval edge_domain = data->curve->Domain();
		if (!data->context)
		    data->context = std::make_shared<brlcad::PullbackContext>();
		const double pullback_tolerance = short_curve_pullback_resolution(
		    data->curve, LocalUnits::tolerance);
		const bool periodic_recovery = data->surf->IsClosed(0) ||
		    data->surf->IsClosed(1);
		ON_3dPoint target_start = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 1 : 0]);
		ON_3dPoint target_end = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 0 : 1]);
		/* Edge geometry can retain a wider proxy domain than a sub-tolerance
		 * topological use.  Its declared STEP vertices are authoritative for
		 * recovery of the lost pullback endpoint on every surface, not only a
		 * periodic one. */
		const int start_vertex = data->edge->m_vi[data->order_reversed ? 1 : 0];
		const int end_vertex = data->edge->m_vi[data->order_reversed ? 0 : 1];
		if (start_vertex >= 0 &&
			start_vertex < brep->m_V.Count())
		    target_start = brep->m_V[start_vertex].point;
		if (end_vertex >= 0 &&
			end_vertex < brep->m_V.Count())
		    target_end = brep->m_V[end_vertex].point;
		ON_2dPoint pulled_start, pulled_end;
		ON_3dPoint pulled_lift;
		double pulled_distance = DBL_MAX;
		/* Preserve an adjacent endpoint when its lift already represents the
		 * declared STEP vertex within model uncertainty.  This keeps the loop
		 * exactly closed in parameter space.  A periodic adjacent edge may begin
		 * at an arbitrary parameter on its 3-D circle, so solve the declared
		 * vertex when that bounded adjacency proof fails. */
		const ON_3dPoint adjacent_lift = data->surf->PointAt(
		    adjacent_start.x, adjacent_start.y);
		const bool adjacent_start_valid = adjacent_lift.IsValid() &&
		    adjacent_lift.DistanceTo(target_start) <= (periodic_recovery ?
			pullback_tolerance : LocalUnits::tolerance);
		if (!adjacent_start_valid &&
		    data->context->SurfaceClosestPoint(data->surf, target_start,
			pulled_start, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    start.Set(pulled_start.x, pulled_start.y, 0.0);
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    start[direction] += round((adjacent_start[direction] -
				start[direction]) / period) * period;
		    }
		}
		ON_3dPoint seeded_end = start;
		if (refine_surface_point_seeded(data->surf, target_end,
			pullback_tolerance, seeded_end, &pulled_distance)) {
		    end = seeded_end;
		} else if (data->context->SurfaceClosestPoint(data->surf, target_end,
			pulled_end, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    end.Set(pulled_end.x, pulled_end.y, 0.0);
		}

		/* Try each closed parameter direction as the constant seam coordinate.
		 * The one-sample pullback has lost its varying endpoint, so recover that
		 * endpoint from the exact 3-D edge and accept a candidate only after a
		 * dense lift comparison with the complete edge. */
		int boundary_direction = -1;
		ON_LineCurve *boundary = NULL;
		bool exact_boundary = false;
		double maximum_boundary_error = 0.0;
		double rejected_boundary_fraction = 0.0;
		const auto validate_boundary = [data, &edge_domain, &target_start,
			&target_end, pullback_tolerance, periodic_recovery](
			ON_LineCurve *candidate, double *maximum_error,
			double *rejected_fraction) {
		    bool candidate_valid = candidate && candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    const ON_Interval candidate_domain = candidate_valid ?
			candidate->Domain() : ON_Interval::EmptyInterval;
		    if (candidate_valid) {
			const ON_3dPoint start_uv = candidate->PointAt(candidate_domain.Min());
			const ON_3dPoint end_uv = candidate->PointAt(candidate_domain.Max());
			const ON_3dPoint start_lift = data->surf->PointAt(
			    start_uv.x, start_uv.y);
			const ON_3dPoint end_lift = data->surf->PointAt(end_uv.x, end_uv.y);
			const double endpoint_tolerance = periodic_recovery ?
			    pullback_tolerance : LocalUnits::tolerance;
			candidate_valid = start_lift.IsValid() && end_lift.IsValid() &&
			    start_lift.DistanceTo(target_start) <= endpoint_tolerance &&
			    end_lift.DistanceTo(target_end) <= endpoint_tolerance;
		    }
		    *maximum_error = 0.0;
		    *rejected_fraction = 0.0;
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const double edge_fraction = data->order_reversed ?
			    1.0 - fraction : fraction;
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(edge_fraction));
			const double error = lifted.IsValid() && target.IsValid() ?
			    lifted.DistanceTo(target) : DBL_MAX;
			*maximum_error = std::max(*maximum_error, error);
			if (error > LocalUnits::tolerance) {
			    *rejected_fraction = fraction;
			    candidate_valid = false;
			}
		    }
		    return candidate_valid;
		};
		if (!data->surf->IsClosed(0) && !data->surf->IsClosed(1)) {
		    ON_LineCurve *candidate = new ON_LineCurve(start, end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    if (validate_boundary(candidate, &candidate_maximum_error,
			    &candidate_rejected_fraction)) {
			boundary = candidate;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = candidate_maximum_error;
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		for (int fixed_direction = 0; fixed_direction < 2 && !exact_boundary;
		     ++fixed_direction) {
		    if (!data->surf->IsClosed(fixed_direction))
			continue;
		    ON_3dPoint candidate_end = end;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    candidate_end[direction] += round((start[direction] -
				candidate_end[direction]) / period) * period;
		    }
		    candidate_end[fixed_direction] = start[fixed_direction];
		    ON_LineCurve *candidate = new ON_LineCurve(start, candidate_end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    const bool candidate_valid = validate_boundary(candidate,
			&candidate_maximum_error, &candidate_rejected_fraction);
		    if (candidate_valid) {
			boundary_direction = fixed_direction;
			boundary = candidate;
			end = candidate_end;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = std::max(maximum_boundary_error,
			    candidate_maximum_error);
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		if (exact_boundary) {
		    const ON_2dPoint original_next_start = (*nsamples)[0];
		    (*nsamples)[0].Set(end.x, end.y);
		    bool next_curve_valid = true;
		    if (next_cs == curve_pullback_samples.begin()) {
			const bool can_rebuild_next = initial_trim_count == 0 &&
			    loop->TrimCount() > 0 &&
			    ndata && ndata->curve && ndata->surf;
			ON_2dPointArray next_interpolation(*nsamples);
			ON_Curve *next_curve = can_rebuild_next ?
			    interpolateCurve(next_interpolation) : NULL;
			const auto make_next_polyline = [nsamples]() -> ON_Curve * {
			    ON_3dPointArray points;
			    for (int point = 0; point < nsamples->Count(); ++point) {
				const ON_3dPoint candidate((*nsamples)[point].x,
				    (*nsamples)[point].y, 0.0);
				if (points.Count() == 0 || candidate.DistanceTo(
					points[points.Count() - 1]) > ON_ZERO_TOLERANCE)
				    points.Append(candidate);
			    }
			    ON_PolylineCurve *polyline = points.Count() >= 2 ?
				new ON_PolylineCurve(points) : NULL;
			    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid())
				return polyline;
			    delete polyline;
			    return NULL;
			};
			const auto validates_next = [ndata](ON_Curve *candidate) {
			    if (!candidate || !candidate->ChangeDimension(2) ||
				    !candidate->IsValid())
				return false;
			    const ON_Interval candidate_domain = candidate->Domain();
			    const ON_Interval next_edge_domain = ndata->curve->Domain();
			    for (int sample = 0;
				    sample <= kDenseLiftValidationSegments; ++sample) {
				if (brlcad::PullbackWorkCancelled()) return false;
				const double fraction = static_cast<double>(sample) /
				    kDenseLiftValidationSegments;
				const ON_3dPoint uv = candidate->PointAt(
				    candidate_domain.ParameterAt(fraction));
				const ON_3dPoint lifted = ndata->surf->PointAt(uv.x, uv.y);
				const ON_3dPoint target = ndata->curve->PointAt(
				    next_edge_domain.ParameterAt(ndata->order_reversed ?
					1.0 - fraction : fraction));
				if (!lifted.IsValid() || lifted.DistanceTo(target) >
					LocalUnits::tolerance)
				    return false;
			    }
			    return true;
			};
			next_curve_valid = can_rebuild_next && validates_next(next_curve);
			if (!next_curve_valid && can_rebuild_next) {
			    delete next_curve;
			    next_curve = make_next_polyline();
			    next_curve_valid = validates_next(next_curve);
			}
			if (next_curve_valid) {
			    ON_BrepTrim *first_trim = loop->Trim(0);
			    const int next_c2 = brep->AddTrimCurve(next_curve);
			    if (!first_trim || next_c2 < 0 ||
				    !brep->SetTrimCurve(*first_trim, next_c2)) {
				if (next_c2 < 0)
				    delete next_curve;
				next_curve_valid = false;
			    } else {
				brep->SetTrimIsoFlags(*first_trim);
			    }
			} else {
			    delete next_curve;
			}
		    }
		    if (next_curve_valid) {
			c2d = boundary;
			const bool periodic_surface = data->surf->IsClosed(0) ||
			    data->surf->IsClosed(1);
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    periodic_surface ?
			    "regenerated a collapsed seam from exact adjacent endpoints" :
			    "regenerated a collapsed boundary pcurve within model tolerance");
		    } else {
			(*nsamples)[0] = original_next_start;
			delete boundary;
		    }
		} else {
		    if (step->Verbose())
			std::cerr << "EDGE_LOOP #" << id
			    << ": collapsed pcurve proof rejected for STEP edge "
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " boundary direction " << boundary_direction
			    << " uv " << start.x << ':' << start.y << " -> "
			    << end.x << ':' << end.y << " at normalized "
			    << rejected_boundary_fraction << " max error "
			    << maximum_boundary_error << " tolerance "
			    << LocalUnits::tolerance << " target length "
			    << target_start.DistanceTo(target_end) << " endpoint errors "
			    << data->surf->PointAt(start.x, start.y).DistanceTo(target_start)
			    << '/' << data->surf->PointAt(end.x, end.y).DistanceTo(target_end)
			    << " surface closed "
			    << (data->surf->IsClosed(0) ? '1' : '0')
			    << (data->surf->IsClosed(1) ? '1' : '0') << " domains "
			    << data->surf->Domain(0).Min() << ':'
			    << data->surf->Domain(0).Max() << ','
			    << data->surf->Domain(1).Min() << ':'
			    << data->surf->Domain(1).Max() << std::endl;
		    delete boundary;
		}
	    }
	    if (!c2d) {
		if (step && step->Verbose())
		    std::cerr << "EDGE_LOOP #" << id
			<< ": could not construct trim for STEP edge "
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " from " << samples->Count() << " pullback samples"
			<< std::endl;
		trim_construction_failed = true;
		break;
	    }
	    trimCurve = brep->m_C2.Count();
	    brep->m_C2.Append(c2d);

	    ON_BrepTrim &trim = brep->NewTrim((ON_BrepEdge &) * data->edge, data->order_reversed, (ON_BrepLoop &) * loop, trimCurve);
	    trim.m_tolerance[0] = LocalUnits::tolerance;
	    trim.m_tolerance[1] = LocalUnits::tolerance;
	    ON_Interval PD = trim.ProxyCurveDomain();
	    trim.m_iso = surface->IsIsoparametric(*c2d, &PD);

	    // check for bridging trim, trims along singularities
	    // are implicitly expected
	    ON_2dPoint end_current, start_next;
	    end_current = (*samples)[samples->Count() - 1];
	    start_next = (*nsamples)[0];

	    if (end_current.DistanceTo(start_next) > LocalUnits::tolerance) {
		// endpoints don't connect
		int is;
		const ON_Surface *surf = data->surf;
		if ((is = check_pullback_singularity_bridge(surf, end_current, start_next)) >= 0) {
		    // insert trim
		    // insert singular trim along
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    ON_Surface::ISO iso = ON_Surface::N_iso;
		    switch (is) {
			case 0:
			    //south
			    iso = ON_Surface::S_iso;
			    break;
			case 1:
			    //east
			    iso = ON_Surface::E_iso;
			    break;
			case 2:
			    //north
			    iso = ON_Surface::N_iso;
			    break;
			case 3:
			    //west
			    iso = ON_Surface::W_iso;
		    }

		    ON_Curve *sing_c2d = new ON_LineCurve(end_current, start_next);
		    trimCurve = brep->m_C2.Count();
		    brep->m_C2.Append(sing_c2d);

		    int vi;
		    if (data->order_reversed) {
			vi = data->edge->m_vi[0];
		    } else {
			vi = data->edge->m_vi[1];
		    }

		    ON_BrepTrim &sing_trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop &) * loop, iso, trimCurve);

		    sing_trim.m_tolerance[0] = LocalUnits::tolerance;
		    sing_trim.m_tolerance[1] = LocalUnits::tolerance;
		    ON_Interval sing_PD = sing_trim.ProxyCurveDomain();
		    sing_trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &sing_PD);
		    sing_trim.m_iso = iso;
		    //trim.Reverse();
		} /*else if ((is = check_pullback_seam_bridge(surf, end_current, start_next)) >= 0) {
		  // insert trim
		  // insert singular trim along
		  // 0 = south, 1 = east, 2 = north, 3 = west
		  ON_Surface::ISO iso;
		  switch (is) {
		  case 0:
		  //south
		  iso = ON_Surface::S_iso;
		  break;
		  case 1:
		  //east
		  iso = ON_Surface::E_iso;
		  break;
		  case 2:
		  //north
		  iso = ON_Surface::N_iso;
		  break;
		  case 3:
		  //west
		  iso = ON_Surface::W_iso;
		  }

		  ON_Curve* c2d = new ON_LineCurve(end_current, start_next);
		  trimCurve = brep->m_C2.Count();
		  brep->m_C2.Append(c2d);

		  int vi;
		  int vo;
		  if (data->order_reversed) {
		  vi = data->edge->m_vi[0];
		  vo = data->edge->m_vi[1];
		  } else {
		  vi = data->edge->m_vi[1];
		  vo = data->edge->m_vi[0];
		  }
		  #ifdef TREATASBOUNDARY
		  ON_BrepEdge& e = (ON_BrepEdge&)*data->edge;
		  ON_BrepTrim& trim = brep->NewTrim(e, false, (ON_BrepLoop&) *loop, trimCurve);

		  trim.m_type = ON_BrepTrim::boundary;
		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  #else
		  ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop&) *loop, iso, trimCurve);

		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  ON_Interval PD = trim.ProxyCurveDomain();
		  trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
		  trim.m_iso = iso;
		  #endif
		  trim.IsValid(&tl);
		  }*/
	    }
	    si++;
	}
    if (trim_construction_failed)
	    break;
	cs++;
    }

    if (step && step->Verbose() && loop->TrimCount() > 1) {
	for (int trim_offset = 0; trim_offset < loop->TrimCount(); ++trim_offset) {
	    const ON_BrepTrim *current_trim = loop->Trim(trim_offset);
	    const ON_BrepTrim *next_trim = loop->Trim((trim_offset + 1) % loop->TrimCount());
	    if (!current_trim || !next_trim)
		continue;
	    const ON_3dPoint current_end = current_trim->PointAtEnd();
	    const ON_3dPoint next_start = next_trim->PointAtStart();
	    if (current_end.DistanceTo(next_start) <= ON_ZERO_TOLERANCE)
		continue;
	    std::cerr << "EDGE_LOOP #" << id << ": trim " << current_trim->m_trim_index
		<< " (edge " << current_trim->m_ei << ", type "
		<< static_cast<int>(current_trim->m_type) << ", iso "
		<< static_cast<int>(current_trim->m_iso) << ") ends at ("
		<< current_end.x << ',' << current_end.y << ") but trim "
		<< next_trim->m_trim_index << " (edge " << next_trim->m_ei
		<< ", type " << static_cast<int>(next_trim->m_type) << ", iso "
		<< static_cast<int>(next_trim->m_iso) << ") starts at ("
		<< next_start.x << ',' << next_start.y << ')' << std::endl;
	}
    }

    while (!curve_pullback_samples.empty()) {
	data = curve_pullback_samples.front();
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
	delete data;
	curve_pullback_samples.pop_front();
    }
    for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	 exact != exact_pullbacks.end(); ++exact)
	delete exact->second;

    if (loop->TrimCount() == initial_trim_count && step && step->Verbose())
	std::cerr << "EDGE_LOOP #" << id << ": trim construction produced no edge uses" << std::endl;

    return !trim_construction_failed;
}


bool
Plane::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	ON_PlaneSurface *s = dynamic_cast<ON_PlaneSurface *>(brep->m_S[ON_id]);

	if (s && trim_curve_3d_bbox) {
	    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();

	    // origin may not lie within face so include in extent
	    double maxdist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_max);
	    double mindist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_min);
	    bbdiag += FMAX(maxdist, mindist);

	    ON_Interval extents(-bbdiag, bbdiag);
	    s->Extend(0,extents);
	    s->Extend(1,extents);
	}

	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    ON_PlaneSurface *s = new ON_PlaneSurface(p);

    if (!trim_curve_3d_bbox) {
	delete s;
	return false;
    }
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    // origin may not lie within face so include in extent
    double maxdist = origin.DistanceTo(trim_curve_3d_bbox->m_max);
    double mindist = origin.DistanceTo(trim_curve_3d_bbox->m_min);
    bbdiag += FMAX(maxdist, mindist);

    //TODO: look into line curves that are just point and direction
    ON_Interval extents(-bbdiag, bbdiag);
    s->SetExtents(0, extents);
    s->SetExtents(1, extents);
    s->SetDomain(0, 0.0, 1.0);
    s->SetDomain(1, 0.0, 1.0);

    ON_id = brep->AddSurface(s);

    return true;
}


bool
CylindricalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // new surface if reused because of bounding
    //if (ON_id >= 0)
    //	return true; // already loaded

    if ((false) && (brep->m_S.Count() == 56)) {
	std::cerr << "LoadONBrep for CylindricalSurface: " << 55 << std::endl;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // make sure origin is part of the bbox
    trim_curve_3d_bbox->Set(origin, true);

    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    origin = origin - bbdiag * norm;
    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    //ON_Cylinder cyl(c, ON_DBL_MAX);
    ON_Cylinder cyl(c, 2.0 * bbdiag);

    ON_RevSurface *s = cyl.RevSurfaceForm();
    if (s) {
	double r = fabs(cyl.circle.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	s->SetDomain(0, 0.0, 2.0 * ON_PI * r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ConicalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double tan_semi_angle = tan(semi_angle * LocalUnits::planeangle);
    double height = (radius * LocalUnits::length) / tan_semi_angle;

    origin = origin + norm * (-height);
    if (NEAR_ZERO(height, BN_TOL_DIST)) {
	// make sure origin is part of the bbox
	trim_curve_3d_bbox->Set(origin, true);

	height = trim_curve_3d_bbox->Diagonal().Length();
    }

    double hplus = height * 2.01;
    double r1 = hplus * tan_semi_angle;

    ON_Plane p(origin, xaxis, yaxis);
    ON_Cone c(p, hplus, r1);

    ON_RevSurface *s = c.RevSurfaceForm();
    if (s) {
	s->SetDomain(0, 0.0, 2.0 * ON_PI);
    }

    ON_id = brep->AddSurface(s);

    return true;
}


int
intersectLines(const ON_Line &l1, const ON_Line &l2, ON_3dPoint &out)
{
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    point_t l1_from, l2_from;
    VMOVE(l1_from, l1.from);
    VMOVE(l2_from, l2.from);

    ON_3dVector d;
    vect_t l1_dir, l2_dir;

    d = l1.Direction();
    VMOVE(l1_dir, d);
    d = l2.Direction();
    VMOVE(l2_dir, d);

    fastf_t l1_dist, l2_dist;
    int i = bg_isect_line3_line3(&l1_dist, &l2_dist, l1_from, l1_dir,
				 l2_from, l2_dir, &tol);
    if (i == 1) {
	ON_3dVector l1_unit_dir = l1.Direction();
	l1_unit_dir.Unitize();

	out = l1.from + l1_unit_dir * l1_dist;
    }
    return i;
}


void
Circle::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    ON_3dPoint P = c.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;


    P = c.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

#define ANGLE_ZERO_TOL 1.0e-6

static double
simplify_angle(double rad)
{
    double result;

    result = fmod(rad, 2.0 * ON_PI);

    if (NEAR_ZERO(result, ANGLE_ZERO_TOL)) {
	result = 0.0;
    } else if (result < 0.0) {
	result += 2.0 * ON_PI;
    }

    return result;
}

static double
radians_from_xaxis_to_ellipse_point(Conic *conic, ON_3dPoint p, double a = 1.0, double b = 1.0)
{
    ON_3dPoint origin(conic->GetOrigin());
    ON_3dVector xaxis(conic->GetXAxis());
    ON_3dVector yaxis(conic->GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // get p after translating to origin
    ON_3dPoint canonical_p = p - origin;

    // decompose into x and y components
    double x = canonical_p * xaxis;
    double y = canonical_p * yaxis;

    // ellipse scaling
    x /= a;
    y /= b;

    return atan2(y, x);
}

bool
Circle::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    if ((false) && ((brep->m_C3.Count() == 3) || (id == 1723))) {
	std::cerr << "Debug:LoadONBrep for Circle:ID:" << id << std::endl;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double r = radius * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle circle(plane, origin, r);

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt);
	s = radians_from_xaxis_to_ellipse_point(this, endpt);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = circle.PointAt(t);
	endpt = circle.PointAt(s);
    }

    double theta = s - t;

    if (VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST)) {
	theta = 2.0 * ON_PI;
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }

    double dtheta = theta / narcs;
    double w = cos(dtheta / 2.0);
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; /* 2 * max narcs + 1 */
    ON_3dPoint circleP1, isect, circleP2;
    ON_3dVector tangentP1, tangentP2;

    circleP1 = circle.PointAt(angle); // was using 'startpt' from edge_curve but found case where not in tol
    tangentP1 = circle.TangentAt(angle);

    for (int i = 0; i < narcs; i++) {
	angle = angle + dtheta;

	circleP2 = circle.PointAt(angle);
	tangentP2 = circle.TangentAt(angle);
	ON_Line tangent1(circleP1, circleP1 + r * tangentP1);
	ON_Line tangent2(circleP2, circleP2 + r * tangentP2);

	if (intersectLines(tangent1, tangent2, isect) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}

	cpts.Append(circleP1);

	isect *= w; // must pre-weight before putting into NURB
	cpts.Append(isect);

	W[2 * i] = 1.0;
	W[2 * i + 1] = w;

	circleP1 = circleP2;
	tangentP1 = tangentP2;
    }
    cpts.Append(circle.PointAt(s));
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Ellipse::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    ON_Ellipse ellipse(plane, a, b);

    ON_3dPoint P = ellipse.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    P = ellipse.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

bool
Ellipse::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane plane(origin, xaxis, yaxis);

    ON_3dPoint center = origin;
    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Ellipse ellipse(plane, a, b);

    // double eccentricity = sqrt(1.0 - (b * b) / (a * a));
    // ON_3dPoint focus_1 = center + (eccentricity * a) * xaxis;
    // ON_3dPoint focus_2 = center - (eccentricity * a) * xaxis;

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt, a, b);
	s = radians_from_xaxis_to_ellipse_point(this, endpt, a, b);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = ellipse.PointAt(t);
	endpt = ellipse.PointAt(s);
    }

    double theta = s - t;

    if (VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST)) {
	theta = 2.0 * ON_PI;
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }
    double dtheta = theta / narcs;
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; // 2 * max narcs + 1
    ON_3dPoint Pnt[2 * 4 + 1];
    ON_3dVector Tangent1, Tangent2;
    ON_3dPoint P0, PX, P2, P1;

    P0 = ellipse.PointAt(angle);
    for (int i = 0; i < narcs; i++) {
	Tangent1 = ellipse.TangentAt(angle);
	PX = ellipse.PointAt(angle + dtheta / 2.0);
	// step angle
	angle = angle + dtheta;
	P2 = ellipse.PointAt(angle);
	Tangent2 = ellipse.TangentAt(angle);
	ON_Line tangent1(P0, P0 + Tangent1);
	ON_Line tangent2(P2, P2 + Tangent2);
	if (intersectLines(tangent1, tangent2, P1) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	ON_Line l1(P1, center);
	ON_Line l2(P0, P2);
	ON_3dPoint PM;
	if (intersectLines(l1, l2, PM) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	double mx = PM.DistanceTo(PX);
	double mp1 = PM.DistanceTo(P1);
	double R = mx / mp1;
	double w = R / (1 - R);

	cpts.Append(P0);
	Pnt[2 * i] = P0;
	W[2 * i] = 1.0;

	Pnt[2 * i + 1] = P1;
	P1 = (w) * P1; // must pre-weight before putting into NURB
	cpts.Append(P1);
	W[2 * i + 1] = w;

	P0 = P2;
    }
    cpts.Append(P2);
    Pnt[2 * narcs] = P2;
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Hyperbola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    ON_3dPoint center = origin;
    double a = semi_axis;
    double b = semi_imag_axis;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }

    double y = b * tan(t);
    double x = a / cos(t);

    ON_3dVector X = x * xaxis;
    ON_3dVector Y = y * yaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    y = b * tan(s);
    x = a / cos(s);

    X = x * xaxis;
    Y = y * yaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Hyperbola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    norm.Unitize();
    xaxis.Unitize();
    yaxis.Unitize();

    ON_3dPoint center = origin * LocalUnits::length;
    double a = semi_axis * LocalUnits::length;
    double b = semi_imag_axis * LocalUnits::length;

    double eccentricity = sqrt(1.0 + (b * b) / (a * a));
    ON_3dPoint focus = center + (eccentricity * a) * xaxis;
    ON_3dPoint focusprime = center - (eccentricity * a) * xaxis;
    // ON_3dPoint vertex = center + a * xaxis;
    // ON_3dPoint directrix = center + (a / eccentricity) * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) {
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 =  pnt2 * LocalUnits::length;

    // calc tangent P0, P2 intersection
    ON_3dVector ToFocus = focus - P0;
    ToFocus.Unitize();

    ON_3dVector ToFocusPrime = focusprime - P0;
    ToFocusPrime.Unitize();

    ON_3dVector bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs0(P0, P0 + bisector);

    ToFocus = focus - P2;
    ToFocus.Unitize();

    ToFocusPrime = focusprime - P2;
    ToFocusPrime.Unitize();

    bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs2(P2, P2 + bisector);
    ON_3dPoint P1;
    if (intersectLines(bs0, bs2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    ON_Line l1(focus, P1);
    ON_Line l2(P0, P2);
    ON_3dPoint M = (P0 + P2) / 2.0;

    ON_Line ctom(center, M);
    ON_3dVector dtom = ctom.Direction();
    dtom.Unitize();
    double m = (dtom * yaxis) / (dtom * xaxis);
    double x1 = a * b * sqrt(1.0 / (b * b - m * m * a * a));
    double y1 = b * sqrt((x1 * x1) / (a * a) - 1.0);
    if (m < 0.0) {
	y1 *= -1.0;
    }
    ON_3dVector X = x1 * xaxis;
    ON_3dVector Y = y1 * yaxis;
    ON_3dPoint Pv = center + X + Y;
    double mx = M.DistanceTo(Pv);
    double mp1 = M.DistanceTo(P1);
    double R = mx / mp1;
    double w = R / (1 - R);

    P1 = (w) * P1; // must pre-weight before putting into NURB

    // add hyperbola weightings
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve *hypernurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*hypernurbscurve);

    ON_id = brep->AddEdgeCurve(hypernurbscurve);

    return true;
}


void
Parabola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    xaxis.Unitize();
    ON_3dVector yaxis(GetYAxis());
    yaxis.Unitize();

    ON_3dPoint center = origin;
    double fd = focal_dist;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    double x = 2.0 * fd * t; // tan(t);
    double y = (x * x) / (4 * fd);

    ON_3dVector X = x * yaxis;
    ON_3dVector Y = y * xaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    x = 2.0 * fd * s; //tan(s);
    y = (x * x) / (4 * fd);

    X = x * yaxis;
    Y = y * xaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Parabola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    // ON_3dVector yaxis(GetYAxis());
    // ON_3dVector normal(GetNormal());

    ON_3dPoint center = origin * LocalUnits::length;
    double fd = focal_dist * LocalUnits::length;
    ON_3dPoint focus = center + fd * xaxis;
    // ON_3dPoint directrix = center - fd * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 = pnt2 * LocalUnits::length;

    // calc tang intersect with transverse axis
    ON_3dVector ToFocus;

    ToFocus = P0 - focus;
    ToFocus.Unitize();
    ON_3dVector bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent1(P0, P0 + 10.0 * bisector);

    ToFocus = P2 - focus;
    ToFocus.Unitize();
    bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent2(P2, P2 + 10.0 * bisector);

    ON_3dPoint P1;
    if (intersectLines(tangent1, tangent2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    // make parabola from bezier
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();

    ON_NurbsCurve *parabnurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*parabnurbscurve);
    ON_id = brep->AddEdgeCurve(parabnurbscurve);

    return true;
}


void
Line::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint ptstart(pnt->Point3d());
    ON_3dVector vdir( dir->Orientation());
    ON_3dPoint ptend = ptstart + (vdir*dir->Magnitude());
    ON_Line l(ptstart, ptend);

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    VMOVE(startpoint,l.PointAt(t));
    VMOVE(endpoint,l.PointAt(s));

    SetPointTrim(startpoint, endpoint);
}


bool
Line::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint startpnt = ON_3dPoint::UnsetPoint;
    ON_3dPoint endpnt = ON_3dPoint::UnsetPoint;

    if (trimmed) { //explicitly trimmed
	startpnt = trim_startpoint;
	endpnt = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	startpnt = start->Point3d();
	endpnt = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    startpnt = startpnt * LocalUnits::length;
    endpnt = endpnt * LocalUnits::length;

    ON_LineCurve *l = new ON_LineCurve(startpnt, endpnt);

    ON_id = brep->AddEdgeCurve(l);

    return true;
}


bool
SurfaceOfLinearExtrusion::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_Curve *curve = brep->m_C3[swept_curve->GetONId()];

    // use trimming edge bounding box unioned with bounding box of
    // curve being extruded; calc diagonal to make sure extrusion
    // magnitude is well represented
    ON_BoundingBox curvebb = curve->BoundingBox();
    trim_curve_3d_bbox->Union(curvebb);
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length(); // already converted to local units;

    ON_3dPoint dir(extrusion_axis->Orientation());
    double mag = extrusion_axis->Magnitude() * LocalUnits::length;
    mag = FMAX(mag, bbdiag);

    ON_3dPoint startpnt;
    if (swept_curve->PointAtStart() == NULL) {
	startpnt = curve->PointAtStart();
    } else {
	startpnt = swept_curve->PointAtStart();
	startpnt = startpnt * LocalUnits::length;
    }

    // add a little buffer in the surface extrusion distance
    // by extruding "+/-mag" distance along "dir"
    ON_3dPoint extrusion_endpnt = startpnt + mag * dir;
    ON_3dPoint extrusion_startpnt = startpnt - mag * dir;

    ON_LineCurve *l = new ON_LineCurve(extrusion_startpnt, extrusion_endpnt);

    // the following extrude code lifted from OpenNURBS ON_BrepExtrude()
    ON_Line path_line;
    path_line.from = extrusion_startpnt;
    path_line.to = extrusion_endpnt;
    ON_3dVector path_vector = path_line.Direction();
    if (path_vector.IsZero()) {
	delete l;
	return false;
    }

    ON_SumSurface *sum_srf = 0;

    ON_Curve *srf_base_curve = curve->Duplicate();
    srf_base_curve->Translate(-mag * dir);

    ON_3dPoint sum_basepoint = ON_origin - l->PointAtStart();
    sum_srf = new ON_SumSurface();

    if (!sum_srf) {
	delete l;
	return false;
    }
    sum_srf->m_curve[0] = srf_base_curve;
    sum_srf->m_curve[1] = l; //srf_path_curve;
    sum_srf->m_basepoint = sum_basepoint;
    sum_srf->BoundingBox(); // fills in sum_srf->m_bbox


    ON_id = brep->AddSurface(sum_srf);

    return true;
}


bool
SurfaceOfRevolution::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_3dPoint start(axis_position->GetOrigin());
    start = start * LocalUnits::length;

    ON_3dVector dir(axis_position->GetNormal());
    ON_3dPoint end = start + dir;

    ON_Line axisline(start, end);
    ON_RevSurface *revsurf = ON_RevSurface::New();

    if (!revsurf) {
	return false;
    }
    revsurf->m_curve = brep->m_C3[swept_curve->GetONId()]->DuplicateCurve();
    revsurf->m_axis = axisline;
    revsurf->BoundingBox(); // fills in sum_srf->m_bbox

    //ON_Brep* b = ON_BrepRevSurface(revsurf, true, true);

    //if (!revsurf)
    //return false;

    ON_id = brep->AddSurface(revsurf);

    return true;
}


bool
SphericalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // get sphere center
    ON_3dPoint center(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    center = center * LocalUnits::length;
    if (!xaxis.Unitize() || !yaxis.Unitize())
	return false;

    // Creates a sphere with given center and radius.
    ON_Sphere sphere(center, radius * LocalUnits::length);
    sphere.plane = ON_Plane(center, xaxis, yaxis);
    if (!sphere.IsValid())
	return false;

    ON_RevSurface *s = sphere.RevSurfaceForm(false, nullptr);
    if (s) {
	double r = fabs(sphere.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	s->SetDomain(1, -r, r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ToroidalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;

    ON_Plane p(origin, xaxis, yaxis);

    // Creates a torus parallel to the plane
    // with given major and minor radius.
    ON_Torus torus(p, major_radius * LocalUnits::length, minor_radius * LocalUnits::length);

    ON_RevSurface *s = torus.RevSurfaceForm();
    if (s) {
	double r = fabs(torus.major_radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	r = fabs(torus.minor_radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	r *= ON_PI;
	s->SetDomain(1, 0.0, 2.0 * r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
VertexLoop::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //load vertex
    loop_vertex->LoadONBrep(brep);

    ON_3dPoint vertex(loop_vertex->Point3d());

    // create singular trim;
    ON_BrepLoop &loop = brep->m_L[ON_loop_index];
    ON_BrepFace *face = loop.Face();
    const ON_Surface *surface = face->SurfaceOf();

    ON_Interval U = surface->Domain(0);
    ON_Interval V = surface->Domain(1);
    ON_3dPoint corner[4];
    corner[0] = surface->PointAt(U.m_t[0], V.m_t[0]);
    corner[1] = surface->PointAt(U.m_t[1], V.m_t[0]);
    corner[2] = surface->PointAt(U.m_t[0], V.m_t[1]);
    corner[3] = surface->PointAt(U.m_t[1], V.m_t[1]);

    ON_2dPoint start, end;
    ON_Surface::ISO iso = ON_Surface::N_iso;
    if (VNEAR_EQUAL(vertex, corner[0], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	    //south;
	    end = ON_2dPoint(U.m_t[1], V.m_t[0]);
	    iso = ON_Surface::S_iso;
	} else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	    //west
	    end = ON_2dPoint(U.m_t[0], V.m_t[1]);
	    iso = ON_Surface::W_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[1], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //east
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::E_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[1]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //north
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::N_iso;
	}
    }
    ON_Curve *c2d = new ON_LineCurve(start, end);
    int trimCurve = brep->m_C2.Count();
    brep->m_C2.Append(c2d);

    (void)brep->NewSingularTrim(brep->m_V[loop_vertex->GetONId()], loop, iso, trimCurve);
    /* FIXME: do something with this trim */

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
