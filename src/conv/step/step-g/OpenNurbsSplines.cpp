/*                    O P E N N U R B S S P L I N E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

/** @file step/OpenNurbsSplines.cpp
 *
 * Exact OpenNURBS construction for STEP spline curve and surface families.
 */

#include "common.h"

#include "sdai.h"
class SDAI_Application_instance;
#include "brep.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "STEPWrapper.h"
#include "LocalUnits.h"
#include "Point.h"
#include "CartesianPoint.h"

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

#include "BezierSurface.h"
#include "BSplineSurface.h"
#include "BSplineSurfaceWithKnots.h"
#include "QuasiUniformSurface.h"
#include "RationalBezierSurface.h"
#include "RationalBSplineSurface.h"
#include "RationalBSplineSurfaceWithKnots.h"
#include "RationalQuasiUniformSurface.h"
#include "RationalUniformSurface.h"
#include "UniformSurface.h"

namespace {

/*
 * ISO 10303 represents a B-spline curve or surface direction with the
 * conventional full knot vector, whose length is cv_count + degree + 1.
 * OpenNURBS stores the same
 * vector without the first and last superfluous knots, so its KnotCount() is
 * two smaller.  In particular, neither omitted knot is necessarily outside
 * [0,1], and omitting an entire first or last multiplicity changes valid
 * unclamped and periodic parameterizations.
 */
static bool
expand_step_knots(int openNURBS_count,
	const LIST_OF_INTEGERS &multiplicities, const LIST_OF_REALS &values,
	bool allow_repeated_values, std::vector<double> *openNURBS_knots,
	size_t *repeated_value_entries, std::string *failure)
{
    if (failure)
	failure->clear();
    if (repeated_value_entries)
	*repeated_value_entries = 0;
    if (!openNURBS_knots) {
	if (failure)
	    *failure = "missing OpenNURBS knot result storage";
	return false;
    }
    openNURBS_knots->clear();
    if (multiplicities.empty() || multiplicities.size() != values.size()) {
	if (failure) {
	    std::ostringstream message;
	    message << "knot_multiplicities has " << multiplicities.size()
		<< " entries but knots has " << values.size();
	    *failure = message.str();
	}
	return false;
    }
    if (openNURBS_count <= 0) {
	if (failure)
	    *failure = "OpenNURBS requested a non-positive knot count";
	return false;
    }
    const size_t full_count = static_cast<size_t>(openNURBS_count) + 2;
    std::vector<double> expanded;
    expanded.reserve(full_count);
    LIST_OF_INTEGERS::const_iterator multiplicity =
	multiplicities.begin();
    LIST_OF_REALS::const_iterator value = values.begin();
    double previous = 0.0;
    bool have_previous = false;
    for (; multiplicity != multiplicities.end();
	    ++multiplicity, ++value) {
	if (*multiplicity <= 0) {
	    if (failure) {
		std::ostringstream message;
		message << "knot multiplicity " << *multiplicity
		    << " is not positive";
		*failure = message.str();
	    }
	    return false;
	}
	const bool repeated_value = have_previous &&
	    !(*value < previous) && !(*value > previous);
	if (!std::isfinite(*value) ||
		(have_previous && *value < previous) ||
		(repeated_value && !allow_repeated_values)) {
	    if (failure) {
		std::ostringstream message;
		message << "distinct STEP knot values are not finite and "
		    "strictly increasing at " << *value;
		*failure = message.str();
	    }
	    return false;
	}
	if (repeated_value && repeated_value_entries)
	    ++*repeated_value_entries;
	have_previous = true;
	previous = *value;
	const size_t repeated_count = static_cast<size_t>(*multiplicity);
	if (expanded.size() > full_count ||
		repeated_count > full_count - expanded.size()) {
	    if (failure) {
		std::ostringstream message;
		message << "expanded STEP knot count exceeds the required "
		    << full_count;
		*failure = message.str();
	    }
	    return false;
	}
	expanded.insert(expanded.end(), repeated_count, *value);
    }
    if (expanded.size() != full_count) {
	if (failure) {
	    std::ostringstream message;
	    message << "expanded STEP knot count " << expanded.size()
		<< " does not equal cv_count + degree + 1 ("
		<< full_count << ')';
	    *failure = message.str();
	}
	return false;
    }

    openNURBS_knots->assign(expanded.begin() + 1, expanded.end() - 1);
    return true;
}


static bool
set_step_curve_knots(ON_NurbsCurve *curve,
	const LIST_OF_INTEGERS &multiplicities, const LIST_OF_REALS &values,
	bool allow_repeated_values, size_t *repeated_value_entries,
	std::string *failure)
{
    if (!curve) {
	if (failure)
	    *failure = "could not allocate the OpenNURBS curve";
	return false;
    }
    std::vector<double> knots;
    if (!expand_step_knots(curve->KnotCount(), multiplicities, values,
	    allow_repeated_values, &knots, repeated_value_entries, failure))
	return false;

    for (int knot_index = 0; knot_index < curve->KnotCount();
	    ++knot_index) {
	if (!curve->SetKnot(knot_index,
		knots[static_cast<size_t>(knot_index)])) {
	    if (failure) {
		std::ostringstream message;
		message << "could not set OpenNURBS knot " << knot_index;
		*failure = message.str();
	    }
	    return false;
	}
    }
    return true;
}


static bool
set_step_surface_knots(ON_NurbsSurface *surface, int direction,
	const LIST_OF_INTEGERS &multiplicities, const LIST_OF_REALS &values,
	bool allow_repeated_values, size_t *repeated_value_entries,
	std::string *failure)
{
    if (!surface || direction < 0 || direction > 1) {
	if (failure)
	    *failure = "invalid OpenNURBS surface knot direction";
	return false;
    }
    std::vector<double> knots;
    if (!expand_step_knots(surface->KnotCount(direction), multiplicities,
	    values, allow_repeated_values, &knots, repeated_value_entries,
	    failure))
	return false;

    for (int knot_index = 0; knot_index < surface->KnotCount(direction);
	    ++knot_index) {
	if (!surface->SetKnot(direction, knot_index,
		knots[static_cast<size_t>(knot_index)])) {
	    if (failure) {
		std::ostringstream message;
		message << "could not set OpenNURBS direction " << direction
		    << " knot " << knot_index;
		*failure = message.str();
	    }
	    return false;
	}
    }
    return true;
}


static bool
validate_step_curve(ON_NurbsCurve *curve, std::string *failure)
{
    if (!curve) {
	if (failure)
	    *failure = "could not allocate the OpenNURBS curve";
	return false;
    }
    ON_wString validation_text;
    ON_TextLog validation_log(validation_text);
    if (curve->IsValid(&validation_log))
	return true;

    ON_String narrow_text(validation_text);
    if (failure) {
	*failure = "constructed OpenNURBS curve is invalid";
	if (narrow_text.Length() > 0) {
	    *failure += ": ";
	    *failure += narrow_text.Array();
	}
    }
    return false;
}


static bool
validate_step_surface(ON_NurbsSurface *surface, std::string *failure)
{
    if (!surface) {
	if (failure)
	    *failure = "could not allocate the OpenNURBS surface";
	return false;
    }
    ON_wString validation_text;
    ON_TextLog validation_log(validation_text);
    if (surface->IsValid(&validation_log))
	return true;

    ON_String narrow_text(validation_text);
    if (failure) {
	*failure = "constructed OpenNURBS surface is invalid";
	if (narrow_text.Length() > 0) {
	    *failure += ": ";
	    *failure += narrow_text.Array();
	}
    }
    return false;
}


static void
record_step_curve_failure(STEPWrapper *step, int64_t id,
	const std::string &failure)
{
    if (step) {
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "B_SPLINE_CURVE_WITH_KNOTS", "knots", failure);
    } else {
	std::cerr << "B_SPLINE_CURVE_WITH_KNOTS #" << id << ": "
	    << failure << std::endl;
    }
}


static void
record_step_surface_failure(STEPWrapper *step, int64_t id,
	const std::string &failure)
{
    if (step) {
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "B_SPLINE_SURFACE_WITH_KNOTS", "knots", failure);
    } else {
	std::cerr << "B_SPLINE_SURFACE_WITH_KNOTS #" << id << ": "
	    << failure << std::endl;
    }
}


static void
record_repeated_step_knot_repair(STEPWrapper *step, int64_t id,
	const char *entity_type, const char *attribute, size_t count)
{
    if (!step || !count)
	return;
    std::ostringstream detail;
    detail << "combined " << count << " repeated entries in the STEP "
	<< attribute << " distinct-value list with their adjacent "
	   "multiplicities without changing the expanded knot vector";
    step->RecordRepair(id, entity_type, attribute, detail.str());
}


/* Positive rational weights are the STEP-conforming sufficient condition for
 * a finite rational surface.  Some writers nevertheless emit a nonpositive
 * interior weight while the tensor-product denominator remains strictly
 * positive.  Prove that narrow case without sampling: when one parameter
 * direction is a clamped linear or quadratic Bezier span, find the exact
 * minimum of every fixed-control-row Bernstein weight polynomial.  The other
 * direction's nonnegative partition-of-unity basis then proves the same lower
 * bound for the complete surface denominator. */
static bool
positive_linear_or_quadratic_bernstein_minimum(
	const std::vector<double> &weights, double *minimum)
{
    if (!minimum || (weights.size() != 2 && weights.size() != 3))
	return false;
    for (size_t i = 0; i < weights.size(); ++i)
	if (!std::isfinite(weights[i])) return false;
    double result = std::min(weights.front(), weights.back());
    if (weights.size() == 3) {
	const double a = weights[0] - 2.0 * weights[1] + weights[2];
	const double b = 2.0 * (weights[1] - weights[0]);
	if (fabs(a) > ON_ZERO_TOLERANCE) {
	    const double stationary = -b / (2.0 * a);
	    if (stationary > 0.0 && stationary < 1.0) {
		const double value =
		    a * stationary * stationary + b * stationary + weights[0];
		result = std::min(result, value);
	    }
	}
    }
    *minimum = result;
    return std::isfinite(result);
}


static bool
rational_surface_denominator_proven_positive(const ON_NurbsSurface *surface,
	double *lower_bound, int *proof_direction)
{
    if (lower_bound) *lower_bound = 0.0;
    if (proof_direction) *proof_direction = -1;
    if (!surface || !surface->IsRational() || !surface->IsValid())
	return false;

    double weight_scale = 0.0;
    for (int u = 0; u < surface->CVCount(0); ++u)
	for (int v = 0; v < surface->CVCount(1); ++v)
	    weight_scale = std::max(weight_scale, fabs(surface->Weight(u, v)));
    if (!(weight_scale > 0.0) || !std::isfinite(weight_scale))
	return false;
    const double positive_margin = weight_scale * ON_SQRT_EPSILON;

    for (int direction = 0; direction < 2; ++direction) {
	const int order = surface->Order(direction);
	if (order < 2 || order > 3 ||
		surface->CVCount(direction) != order ||
		!surface->IsClamped(direction))
	    continue;
	const int fixed_direction = 1 - direction;
	double direction_minimum = DBL_MAX;
	bool proven = true;
	for (int fixed = 0; fixed < surface->CVCount(fixed_direction);
		++fixed) {
	    std::vector<double> weights;
	    weights.reserve(static_cast<size_t>(order));
	    for (int varying = 0; varying < order; ++varying) {
		const int u = direction == 0 ? varying : fixed;
		const int v = direction == 0 ? fixed : varying;
		weights.push_back(surface->Weight(u, v));
	    }
	    double row_minimum = 0.0;
	    if (!positive_linear_or_quadratic_bernstein_minimum(weights,
		    &row_minimum) || row_minimum <= positive_margin) {
		proven = false;
		break;
	    }
	    direction_minimum = std::min(direction_minimum, row_minimum);
	}
	if (proven) {
	    if (lower_bound) *lower_bound = direction_minimum;
	    if (proof_direction) *proof_direction = direction;
	    return true;
	}
    }
    return false;
}


static void
record_nonpositive_weight_proof(STEPWrapper *step, int64_t id,
	double lower_bound, int proof_direction)
{
    if (!step)
	return;
    std::ostringstream detail;
    detail << "preserved the exact rational surface after analytically "
	       "proving its homogeneous denominator remains positive (lower "
	       "bound " << lower_bound << ", "
	       << (proof_direction == 0 ? 'u' : 'v')
	       << "-direction Bezier rows)";
    step->RecordRepair(id, "RATIONAL_B_SPLINE_SURFACE", "weights_data",
	detail.str());
    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, id,
	"RATIONAL_B_SPLINE_SURFACE", "weights_data",
	"source weights include a nonpositive value; " + detail.str());
}


/* STEP's u_closed/v_closed attributes assert geometric closure independently
 * of the particular knot form.  OpenNURBS recognizes a closed NURBS surface
 * only when it is clamped with coincident end CVs or encoded in OpenNURBS'
 * periodic form.  Clamp the declared direction and make its numerically equal
 * boundary CVs bit-identical.  A dense before/after audit limits this to the
 * file uncertainty; ordinary floating-point normalization remains available
 * when repairs are disabled because it does not change the represented locus.
 */
static bool
close_declared_surface_direction(ON_NurbsSurface *surface, int direction,
	STEPWrapper *step, int64_t id)
{
    if (!surface || direction < 0 || direction > 1 ||
	    surface->IsClosed(direction))
	return true;
    const ON_Interval closed_domain = surface->Domain(direction);
    const ON_Interval other_domain = surface->Domain(1 - direction);
    if (!closed_domain.IsIncreasing() || !other_domain.IsIncreasing())
	return false;

    constexpr int kSurfaceAuditSegments = 64;
    double seam_discrepancy = 0.0;
    for (int sample = 0; sample <= kSurfaceAuditSegments; ++sample) {
	const double other = other_domain.ParameterAt(
	    static_cast<double>(sample) / kSurfaceAuditSegments);
	const ON_3dPoint start = direction == 0 ?
	    surface->PointAt(closed_domain.Min(), other) :
	    surface->PointAt(other, closed_domain.Min());
	const ON_3dPoint end = direction == 0 ?
	    surface->PointAt(closed_domain.Max(), other) :
	    surface->PointAt(other, closed_domain.Max());
	if (!start.IsValid() || !end.IsValid())
	    return false;
	seam_discrepancy = std::max(seam_discrepancy,
	    start.DistanceTo(end));
    }

    const double numerical_limit = ON_ZERO_TOLERANCE * 64.0;
    const double declared_tolerance = std::max(LocalUnits::tolerance,
	numerical_limit);
    const bool numerical_normalization =
	seam_discrepancy <= numerical_limit;
    if (!numerical_normalization &&
	    (!step || step->ImportOptions().repair !=
		brlcad::step::RepairMode::Safe))
	return false;
    const double accepted_limit = numerical_normalization ?
	numerical_limit : declared_tolerance;
    if (!std::isfinite(seam_discrepancy) ||
	    seam_discrepancy > accepted_limit)
	return false;

    ON_NurbsSurface candidate(*surface);
    if (!candidate.ClampEnd(direction, 2))
	return false;
    const int closed_count = candidate.CVCount(direction);
    const int other_count = candidate.CVCount(1 - direction);
    if (closed_count < 2 || other_count < 1)
	return false;
    for (int other = 0; other < other_count; ++other) {
	const int start_u = direction == 0 ? 0 : other;
	const int start_v = direction == 0 ? other : 0;
	const int end_u = direction == 0 ? closed_count - 1 : other;
	const int end_v = direction == 0 ? other : closed_count - 1;
	ON_4dPoint start;
	ON_4dPoint end;
	if (!candidate.GetCV(start_u, start_v, start) ||
		!candidate.GetCV(end_u, end_v, end))
	    return false;
	const ON_4dPoint average((start.x + end.x) * 0.5,
	    (start.y + end.y) * 0.5, (start.z + end.z) * 0.5,
	    (start.w + end.w) * 0.5);
	if (!(average.w > 0.0) || !std::isfinite(average.w) ||
		!candidate.SetCV(start_u, start_v, average) ||
		!candidate.SetCV(end_u, end_v, average))
	    return false;
    }
    if (!candidate.IsClosed(direction) || !candidate.IsValid())
	return false;

    double maximum_change = 0.0;
    for (int u_sample = 0; u_sample <= kSurfaceAuditSegments; ++u_sample) {
	const double u = surface->Domain(0).ParameterAt(
	    static_cast<double>(u_sample) / kSurfaceAuditSegments);
	for (int v_sample = 0; v_sample <= kSurfaceAuditSegments; ++v_sample) {
	    const double v = surface->Domain(1).ParameterAt(
		static_cast<double>(v_sample) / kSurfaceAuditSegments);
	    const ON_3dPoint original = surface->PointAt(u, v);
	    const ON_3dPoint closed = candidate.PointAt(u, v);
	    if (!original.IsValid() || !closed.IsValid())
		return false;
	    maximum_change = std::max(maximum_change,
		original.DistanceTo(closed));
	}
    }
    if (!std::isfinite(maximum_change) || maximum_change > accepted_limit)
	return false;

    *surface = candidate;
    std::ostringstream detail;
    detail << "closed the declared STEP surface direction " << direction
	<< " after measuring seam discrepancy " << seam_discrepancy
	<< " mm and maximum surface change " << maximum_change << " mm";
    if (step) {
	step->RecordRepair(id, "B_SPLINE_SURFACE_WITH_KNOTS",
	    direction == 0 ? "u_closed" : "v_closed", detail.str());
    }
    return true;
}

}

bool
BezierCurve::LoadONBrep(ON_Brep *brep)
{
    /* STEP omits the knot vector because a Bezier curve is precisely a
     * clamped B-spline with no interior knots. */
    return BSplineCurve::LoadONBrep(brep);
}


bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
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
    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    const int t_size = static_cast<int>(control_points_list.size());
    if (degree < 1 || t_size < degree + 1) {
	std::ostringstream failure;
	failure << "degree " << degree << " and control point count "
	    << t_size << " cannot define a B-spline curve";
	record_step_curve_failure(step, id, failure.str());
	return false;
    }

    std::unique_ptr<ON_NurbsCurve> curve(
	ON_NurbsCurve::New(3, false, degree + 1, t_size));
    std::string failure;
    size_t repeated_knot_entries = 0;
    const bool canonicalize_repeated_knots = step &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    if (!set_step_curve_knots(curve.get(), knot_multiplicities, knots,
	    canonicalize_repeated_knots, &repeated_knot_entries, &failure)) {
	record_step_curve_failure(step, id, failure);
	return false;
    }
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_CURVE_WITH_KNOTS", "knots", repeated_knot_entries);

    LIST_OF_POINTS::const_iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	if (!*i || !curve->SetCV(cv_index,
		ON_3dPoint((*i)->X() * LocalUnits::length,
		    (*i)->Y() * LocalUnits::length,
		    (*i)->Z() * LocalUnits::length))) {
	    std::ostringstream message;
	    message << "could not set OpenNURBS control point " << cv_index;
	    record_step_curve_failure(step, id, message.str());
	    return false;
	}
	cv_index++;
    }

    if (!validate_step_curve(curve.get(), &failure)) {
	record_step_curve_failure(step, id, failure);
	return false;
    }
    SetONId(brep->AddEdgeCurve(curve.release()));

    return true;
}


bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
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
    return RationalBSplineCurve::LoadONBrep(brep);
}


bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
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

    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    const int t_size = static_cast<int>(control_points_list.size());
    if (degree < 1 || t_size < degree + 1) {
	std::ostringstream failure;
	failure << "degree " << degree << " and control point count "
	    << t_size << " cannot define a rational B-spline curve";
	record_step_curve_failure(step, id, failure.str());
	return false;
    }
    if (weights_data.size() != control_points_list.size()) {
	std::ostringstream failure;
	failure << "weight count " << weights_data.size()
	    << " does not equal control point count " << t_size;
	record_step_curve_failure(step, id, failure.str());
	return false;
    }

    std::unique_ptr<ON_NurbsCurve> curve(
	ON_NurbsCurve::New(3, true, degree + 1, t_size));
    std::string failure;
    size_t repeated_knot_entries = 0;
    const bool canonicalize_repeated_knots = step &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    if (!set_step_curve_knots(curve.get(), knot_multiplicities, knots,
	    canonicalize_repeated_knots, &repeated_knot_entries, &failure)) {
	record_step_curve_failure(step, id, failure);
	return false;
    }
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_CURVE_WITH_KNOTS", "knots", repeated_knot_entries);

    LIST_OF_POINTS::const_iterator i;
    LIST_OF_REALS::const_iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	if (!*i || !std::isfinite(w) ||
		!curve->SetCV(cv_index,
		    ON_4dPoint((*i)->X() * LocalUnits::length * w,
			(*i)->Y() * LocalUnits::length * w,
			(*i)->Z() * LocalUnits::length * w, w))) {
	    std::ostringstream message;
	    message << "could not set rational OpenNURBS control point "
		<< cv_index;
	    record_step_curve_failure(step, id, message.str());
	    return false;
	}
	cv_index++;
	r++;
    }

    if (!validate_step_curve(curve.get(), &failure)) {
	record_step_curve_failure(step, id, failure);
	return false;
    }
    SetONId(brep->AddEdgeCurve(curve.release()));

    return true;
}


bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
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

    if (GetONId() >= 0) {
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

    if (GetONId() >= 0) {
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
    return BSplineSurface::LoadONBrep(brep);
}


bool
BSplineSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !control_points_list || control_points_list->empty() ||
	    !control_points_list->front())
	return false;
    if (GetONId() >= 0)
	return true;
    const int u_size = control_points_list->size();
    const int v_size = control_points_list->front()->size();
    if (u_degree < 1 || v_degree < 1 || u_size < u_degree + 1 ||
	    v_size < v_degree + 1)
	return false;
    for (LIST_OF_LIST_OF_POINTS::const_iterator row =
	    control_points_list->begin(); row != control_points_list->end(); ++row)
	if (!*row || static_cast<int>((*row)->size()) != v_size)
	    return false;

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);
    if (!surf)
	return false;

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
	    if (!*j) {
		delete surf;
		return false;
	    }
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
	    v++;
	}
	u++;
    }
    SetONId(brep->AddSurface(surf));

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
    if (!brep || !control_points_list || control_points_list->empty() ||
	    !control_points_list->front())
	return false;
    if (GetONId() >= 0)
	return true;
    const int u_size = static_cast<int>(control_points_list->size());
    const int v_size = static_cast<int>(control_points_list->front()->size());
    if (u_degree < 1 || v_degree < 1 || u_size < u_degree + 1 ||
	    v_size < v_degree + 1) {
	record_step_surface_failure(step, id,
	    "degree and control-point counts cannot define a B-spline surface");
	return false;
    }
    for (LIST_OF_LIST_OF_POINTS::const_iterator row =
	    control_points_list->begin(); row != control_points_list->end(); ++row) {
	if (!*row || static_cast<int>((*row)->size()) != v_size) {
	    record_step_surface_failure(step, id,
		"control-point rows do not have a consistent length");
	    return false;
	}
    }

    std::unique_ptr<ON_NurbsSurface> surf(ON_NurbsSurface::New(3, false,
	u_degree + 1, v_degree + 1, u_size, v_size));
    std::string failure;
    const bool canonicalize_repeated_knots = step &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    size_t repeated_u_knot_entries = 0;
    size_t repeated_v_knot_entries = 0;
    if (!set_step_surface_knots(surf.get(), 0, u_multiplicities, u_knots,
	    canonicalize_repeated_knots, &repeated_u_knot_entries, &failure) ||
	    !set_step_surface_knots(surf.get(), 1, v_multiplicities, v_knots,
		canonicalize_repeated_knots, &repeated_v_knot_entries,
		&failure)) {
	record_step_surface_failure(step, id, failure);
	return false;
    }
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_SURFACE_WITH_KNOTS", "u_knots",
	repeated_u_knot_entries);
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_SURFACE_WITH_KNOTS", "v_knots",
	repeated_v_knot_entries);

    int u = 0;
    for (LIST_OF_LIST_OF_POINTS::const_iterator row =
	    control_points_list->begin(); row != control_points_list->end();
	    ++row, ++u) {
	int v = 0;
	for (LIST_OF_POINTS::const_iterator point = (*row)->begin();
		point != (*row)->end(); ++point, ++v) {
	    if (!*point || !surf->SetCV(u, v, ON_3dPoint(
		    (*point)->X() * LocalUnits::length,
		    (*point)->Y() * LocalUnits::length,
		    (*point)->Z() * LocalUnits::length))) {
		record_step_surface_failure(step, id,
		    "could not set an OpenNURBS surface control point");
		return false;
	    }
	    }
    }
    if (u_closed)
	(void)close_declared_surface_direction(surf.get(), 0, step, id);
    if (v_closed)
	(void)close_declared_surface_direction(surf.get(), 1, step, id);
    if (!validate_step_surface(surf.get(), &failure)) {
	record_step_surface_failure(step, id, failure);
	return false;
    }
    SetONId(brep->AddSurface(surf.release()));

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
    return RationalBSplineSurface::LoadONBrep(brep);
}


bool
RationalBSplineSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !control_points_list || control_points_list->empty() ||
	    !control_points_list->front())
	return false;
    if (GetONId() >= 0)
	return true;
    const int u_size = control_points_list->size();
    const int v_size = control_points_list->front()->size();
    if (u_degree < 1 || v_degree < 1 || u_size < u_degree + 1 ||
	    v_size < v_degree + 1)
	return false;
    if (weights_data.size() != control_points_list->size())
	return false;
    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, true,
	u_degree + 1, v_degree + 1, u_size, v_size);
    if (!surf)
	return false;

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
    bool has_nonpositive_weight = false;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	if (!pnts || static_cast<int>(pnts->size()) != v_size ||
		w == weights_data.end() || !*w ||
		(*w)->size() != pnts->size()) {
	    delete surf;
	    return false;
	}
	r = (*w)->begin();
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j, r++, v++) {
	    double weight = (*r);
	    if (!*j || !std::isfinite(weight) ||
		    !surf->SetCV(u, v, ON_4dPoint(
			(*j)->X() * LocalUnits::length * weight,
			(*j)->Y() * LocalUnits::length * weight,
			(*j)->Z() * LocalUnits::length * weight, weight))) {
		delete surf;
		return false;
	    }
	    if (!(weight > 0.0))
		has_nonpositive_weight = true;
	}
	u++;
	w++;
    }
    if (has_nonpositive_weight) {
	double denominator_lower_bound = 0.0;
	int proof_direction = -1;
	const bool safe_mode = step &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	if (!safe_mode || !rational_surface_denominator_proven_positive(surf,
		&denominator_lower_bound, &proof_direction)) {
	    delete surf;
	    return false;
	}
	record_nonpositive_weight_proof(step, id, denominator_lower_bound,
	    proof_direction);
    }
    SetONId(brep->AddSurface(surf));

    return true;
}


bool
RationalBSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !control_points_list || control_points_list->empty() ||
	    !control_points_list->front())
	return false;
    if (GetONId() >= 0)
	return true;
    const int u_size = static_cast<int>(control_points_list->size());
    const int v_size = static_cast<int>(control_points_list->front()->size());
    if (u_degree < 1 || v_degree < 1 || u_size < u_degree + 1 ||
	    v_size < v_degree + 1) {
	record_step_surface_failure(step, id,
	    "degree and control-point counts cannot define a rational B-spline surface");
	return false;
    }
    if (weights_data.size() != control_points_list->size()) {
	record_step_surface_failure(step, id,
	    "weight rows do not match the control-point rows");
	return false;
    }

    std::unique_ptr<ON_NurbsSurface> surf(ON_NurbsSurface::New(3, true,
	u_degree + 1, v_degree + 1, u_size, v_size));
    std::string failure;
    const bool canonicalize_repeated_knots = step &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    size_t repeated_u_knot_entries = 0;
    size_t repeated_v_knot_entries = 0;
    if (!set_step_surface_knots(surf.get(), 0, u_multiplicities, u_knots,
	    canonicalize_repeated_knots, &repeated_u_knot_entries, &failure) ||
	    !set_step_surface_knots(surf.get(), 1, v_multiplicities, v_knots,
		canonicalize_repeated_knots, &repeated_v_knot_entries,
		&failure)) {
	record_step_surface_failure(step, id, failure);
	return false;
    }
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_SURFACE_WITH_KNOTS", "u_knots",
	repeated_u_knot_entries);
    record_repeated_step_knot_repair(step, id,
	"B_SPLINE_SURFACE_WITH_KNOTS", "v_knots",
	repeated_v_knot_entries);

    LIST_OF_LIST_OF_REALS::const_iterator weight_row = weights_data.begin();
    int u = 0;
    bool has_nonpositive_weight = false;
    for (LIST_OF_LIST_OF_POINTS::const_iterator row =
	    control_points_list->begin(); row != control_points_list->end();
	    ++row, ++weight_row, ++u) {
	if (!*row || static_cast<int>((*row)->size()) != v_size ||
		weight_row == weights_data.end() || !*weight_row ||
		(*weight_row)->size() != (*row)->size()) {
	    record_step_surface_failure(step, id,
		"control-point and weight rows do not have consistent lengths");
	    return false;
	}
	LIST_OF_REALS::const_iterator weight = (*weight_row)->begin();
	int v = 0;
	for (LIST_OF_POINTS::const_iterator point = (*row)->begin();
		point != (*row)->end(); ++point, ++weight, ++v) {
	    if (!*point || !std::isfinite(*weight) ||
		    !surf->SetCV(u, v, ON_4dPoint(
			(*point)->X() * LocalUnits::length * *weight,
			(*point)->Y() * LocalUnits::length * *weight,
			(*point)->Z() * LocalUnits::length * *weight,
			*weight))) {
		record_step_surface_failure(step, id,
		    "could not set a finite rational OpenNURBS control point");
		return false;
	    }
	    if (!(*weight > 0.0))
		has_nonpositive_weight = true;
	    }
    }
    if (has_nonpositive_weight) {
	double denominator_lower_bound = 0.0;
	int proof_direction = -1;
	const bool safe_mode = step &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	if (!safe_mode || !rational_surface_denominator_proven_positive(
		surf.get(), &denominator_lower_bound, &proof_direction)) {
	    record_step_surface_failure(step, id,
		"source rational weights include a nonpositive value and the complete surface denominator could not be proven positive");
	    return false;
	}
	record_nonpositive_weight_proof(step, id, denominator_lower_bound,
	    proof_direction);
    }
    if (u_closed)
	(void)close_declared_surface_direction(surf.get(), 0, step, id);
    if (v_closed)
	(void)close_declared_surface_direction(surf.get(), 1, step, id);
    if (!validate_step_surface(surf.get(), &failure)) {
	record_step_surface_failure(step, id, failure);
	return false;
    }
    SetONId(brep->AddSurface(surf.release()));

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



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
