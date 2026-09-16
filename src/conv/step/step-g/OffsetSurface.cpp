/*                 OffsetSurface.cpp
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
/** @file step/OffsetSurface.cpp
 *
 * Routines to interface to STEP "OffsetSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "OffsetSurface.h"
#include "LocalUnits.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

struct OffsetProfileSample {
    double parameter;
    ON_3dPoint point;
};


static bool
sum_surface_offset_profile_point(const ON_SumSurface *surface,
	double profile_parameter, double path_parameter, double distance,
	ON_3dPoint *point)
{
    if (!surface || !surface->m_curve[0] || !point)
	return false;
    ON_3dPoint surface_point;
    ON_3dVector profile_derivative, path_derivative, normal;
    if (!surface->EvNormal(profile_parameter, path_parameter, surface_point,
	    profile_derivative, path_derivative, normal) ||
	    !normal.Unitize())
	return false;
    const ON_3dPoint profile_point =
	surface->m_curve[0]->PointAt(profile_parameter);
    if (!profile_point.IsValid())
	return false;
    *point = profile_point + distance * normal;
    return point->IsValid();
}


template <typename OffsetEvaluator>
static bool
append_offset_profile_span(OffsetEvaluator &evaluate_offset,
	double tolerance,
	const OffsetProfileSample &start, const OffsetProfileSample &end,
	int depth, std::vector<OffsetProfileSample> *samples)
{
    if (!samples || depth > 20 || samples->size() >= 65536)
	return false;
    const double fractions[3] = {0.25, 0.5, 0.75};
    OffsetProfileSample probes[3];
    bool bounded = true;
    for (int i = 0; i < 3; ++i) {
	probes[i].parameter = start.parameter + fractions[i] *
	    (end.parameter - start.parameter);
	if (!evaluate_offset(probes[i].parameter, &probes[i].point))
	    return false;
	const ON_3dPoint chord = start.point + fractions[i] *
	    (end.point - start.point);
	bounded = bounded && probes[i].point.DistanceTo(chord) <= tolerance;
    }
    if (bounded) {
	samples->push_back(end);
	return true;
    }
    if (depth == 20)
	return false;
    return append_offset_profile_span(evaluate_offset, tolerance, start,
	probes[1], depth + 1, samples) &&
	append_offset_profile_span(evaluate_offset, tolerance, probes[1], end,
	    depth + 1, samples);
}


template <typename OffsetEvaluator>
static ON_PolylineCurve *
tolerance_bounded_offset_profile(const ON_Curve *profile_curve,
	OffsetEvaluator &evaluate_offset, double tolerance,
	double *maximum_deviation, size_t *segment_count,
	std::string *failure)
{
    if (failure)
	failure->clear();
    if (maximum_deviation)
	*maximum_deviation = DBL_MAX;
    if (segment_count)
	*segment_count = 0;

    if (!profile_curve || !(tolerance > 0.0) ||
	    !profile_curve->Domain().IsIncreasing()) {
	if (failure)
	    *failure = "the source profile or its parameter domain was invalid";
	return NULL;
    }
    const int span_count = profile_curve->SpanCount();
    if (span_count < 1) {
	if (failure)
	    *failure = "the source profile had no evaluable spans";
	return NULL;
    }
    std::vector<double> span_parameters(span_count + 1, 0.0);
    if (!profile_curve->GetSpanVector(span_parameters.data())) {
	if (failure)
	    *failure = "the source profile span vector was unavailable";
	return NULL;
    }
    const double approximation_tolerance = std::max(ON_ZERO_TOLERANCE,
	0.25 * tolerance);
    std::vector<OffsetProfileSample> samples;
    samples.reserve(std::min(65536, span_count * 16 + 1));
    for (int span = 0; span < span_count; ++span) {
	OffsetProfileSample start = {span_parameters[span],
	    ON_3dPoint::UnsetPoint};
	OffsetProfileSample end = {span_parameters[span + 1],
	    ON_3dPoint::UnsetPoint};
	if (!evaluate_offset(start.parameter, &start.point) ||
	    !evaluate_offset(end.parameter, &end.point)) {
	    if (failure)
		*failure = "an offset profile span endpoint could not be evaluated";
	    return NULL;
	}
	if (samples.empty())
	    samples.push_back(start);
	else {
	    if (samples.back().point.DistanceTo(start.point) >
		    approximation_tolerance) {
		if (failure)
		    *failure = "adjacent offset profile spans were discontinuous";
		return NULL;
	    }
	    /* Adjacent source spans share one exact parameter.  Keep one literal
	     * profile vertex so numerical one-sided normal evaluation cannot put a
	     * microscopic crack into the polyline surface. */
	    start.point = samples.back().point;
	}
	if (!append_offset_profile_span(evaluate_offset,
		approximation_tolerance, start, end, 0, &samples)) {
	    if (failure)
		*failure = "adaptive offset profile refinement exceeded its bounds";
	    return NULL;
	}
    }
    if (samples.size() < 2 || samples.size() > 65536) {
	if (failure)
	    *failure = "the bounded offset profile had an invalid sample count";
	return NULL;
    }
    if (profile_curve->IsClosed()) {
	if (samples.front().point.DistanceTo(samples.back().point) >
		approximation_tolerance) {
	    if (failure)
		*failure = "the offset of a closed profile did not close";
	    return NULL;
	}
	samples.back().point = samples.front().point;
    }

    ON_3dPointArray points;
    ON_SimpleArray<double> parameters;
    points.Reserve(static_cast<int>(samples.size()));
    parameters.Reserve(static_cast<int>(samples.size()));
    for (size_t i = 0; i < samples.size(); ++i) {
	points.Append(samples[i].point);
	parameters.Append(samples[i].parameter);
    }
    std::unique_ptr<ON_PolylineCurve> profile(
	new ON_PolylineCurve(points, parameters));
    if (!profile || !profile->IsValid()) {
	if (failure)
	    *failure = "the tolerance-bounded offset profile was invalid";
	return NULL;
    }

    double measured_deviation = 0.0;
    for (size_t interval = 1; interval < samples.size(); ++interval) {
	for (int sample = 0; sample <= 8; ++sample) {
	    const double fraction = static_cast<double>(sample) / 8.0;
	    const double parameter = samples[interval - 1].parameter +
		fraction * (samples[interval].parameter -
		    samples[interval - 1].parameter);
	    ON_3dPoint exact_profile;
	    if (!evaluate_offset(parameter, &exact_profile)) {
		if (failure)
		    *failure = "an offset profile validation point could not be evaluated";
		return NULL;
	    }
	    const ON_3dPoint candidate_profile = profile->PointAt(parameter);
	    const double deviation = exact_profile.IsValid() &&
		candidate_profile.IsValid() ?
		exact_profile.DistanceTo(candidate_profile) : DBL_MAX;
	    measured_deviation = std::max(measured_deviation, deviation);
	    if (deviation > tolerance) {
		if (failure)
		    *failure = "the represented profile exceeded the STEP uncertainty";
		return NULL;
	    }
	}
    }
    if (maximum_deviation)
	*maximum_deviation = measured_deviation;
    if (segment_count)
	*segment_count = samples.size() - 1;
    return profile.release();
}


/* An offset of a linear extrusion is another linear extrusion.  Its profile
 * is c(u) + d*unit(c'(u) x path'), which is generally not rational even when
 * c is a NURBS (an ellipse is the common case).  Approximate only that
 * one-dimensional profile, preserve the exact sweep, and require the complete
 * result to remain inside the STEP file uncertainty.  This is both tighter
 * and substantially smaller than fitting an unconstrained two-dimensional
 * offset surface. */
static ON_SumSurface *
offset_linear_extrusion(const ON_SumSurface *surface, double distance,
	double tolerance, double *maximum_deviation, size_t *segment_count,
	std::string *failure)
{
    if (maximum_deviation)
	*maximum_deviation = DBL_MAX;
    if (segment_count)
	*segment_count = 0;
    if (!surface || !surface->m_curve[0] || !surface->m_curve[1] ||
	    !ON_LineCurve::Cast(surface->m_curve[1]) ||
	    !(tolerance > 0.0))
	return NULL;
    const ON_Interval profile_domain = surface->m_curve[0]->Domain();
    const ON_Interval path_domain = surface->m_curve[1]->Domain();
    if (!profile_domain.IsIncreasing() || !path_domain.IsIncreasing())
	return NULL;
    const double path_parameter = path_domain.Mid();
    auto evaluate_offset = [surface, path_parameter, distance](
	double parameter, ON_3dPoint *point) {
	return sum_surface_offset_profile_point(surface, parameter,
	    path_parameter, distance, point);
    };
    std::unique_ptr<ON_PolylineCurve> profile(
	tolerance_bounded_offset_profile(surface->m_curve[0],
	    evaluate_offset, tolerance, maximum_deviation, segment_count,
	    failure));
    if (!profile)
	return NULL;

    std::unique_ptr<ON_SumSurface> candidate(new ON_SumSurface(*surface));
    if (!candidate || !candidate->m_curve[0])
	return NULL;
    delete candidate->m_curve[0];
    candidate->m_curve[0] = profile.release();
    candidate->m_bbox = ON_BoundingBox::EmptyBoundingBox;
    candidate->BoundingBox();
    if (!candidate->IsValid())
	return NULL;
    return candidate.release();
}


static bool
revolution_offset_profile_point(const ON_RevSurface *surface,
	double profile_parameter, double distance, ON_3dPoint *point)
{
    if (!surface || !surface->m_curve || !point)
	return false;

    /* Construct the normal in the revolution's stored profile frame.  Calling
     * EvNormal at an integral-turn parameter is needlessly fragile: that
     * parameter is also the angular seam, and at a profile endpoint OpenNURBS
     * must resolve two simultaneous one-sided derivatives.  The analytic
     * derivatives are the same ones used by ON_RevSurface::Evaluate and work
     * for partial revolutions whose angle interval does not include zero. */
    const ON_Interval domain = surface->m_curve->Domain();
    int side = 0;
    if (profile_parameter <= domain.Min())
	side = 1;
    else if (profile_parameter >= domain.Max())
	side = -1;
    ON_3dPoint profile_point;
    ON_3dVector profile_derivative;
    if (!surface->m_curve->Ev1Der(profile_parameter, profile_point,
	    profile_derivative, side) || !profile_derivative.Unitize())
	return false;
    ON_3dVector axis = surface->m_axis.Tangent();
    if (!axis.Unitize() || !profile_point.IsValid())
	return false;
    const ON_3dPoint axis_point = surface->m_axis.ClosestPointTo(profile_point);
    ON_3dVector radial = profile_point - axis_point;
    if (!radial.Unitize())
	return false;
    ON_3dVector angular_derivative = ON_CrossProduct(axis, radial);
    ON_3dVector normal = surface->m_bTransposed ?
	ON_CrossProduct(profile_derivative, angular_derivative) :
	ON_CrossProduct(angular_derivative, profile_derivative);
    if (!normal.Unitize())
	return false;
    *point = profile_point + distance * normal;
    return point->IsValid();
}


/* A surface of revolution also reduces to a one-dimensional offset profile.
 * Keep the exact axis, angle interval, parameter domains, and transpose state;
 * only the generally non-rational meridian offset is tolerance-bounded. */
static ON_RevSurface *
offset_surface_of_revolution(const ON_RevSurface *surface, double distance,
	double tolerance, double *maximum_deviation, size_t *segment_count,
	std::string *failure)
{
    if (maximum_deviation)
	*maximum_deviation = DBL_MAX;
    if (segment_count)
	*segment_count = 0;
    if (!surface || !surface->m_curve || !(tolerance > 0.0) ||
	    !surface->m_angle.IsIncreasing() || !surface->m_t.IsIncreasing())
	return NULL;
    auto evaluate_offset = [surface, distance](double parameter,
	ON_3dPoint *point) {
	return revolution_offset_profile_point(surface, parameter, distance,
	    point);
    };
    std::unique_ptr<ON_PolylineCurve> profile(
	tolerance_bounded_offset_profile(surface->m_curve, evaluate_offset,
	    tolerance, maximum_deviation, segment_count, failure));
    if (!profile)
	return NULL;
    std::unique_ptr<ON_RevSurface> candidate(new ON_RevSurface(*surface));
    if (!candidate || !candidate->m_curve)
	return NULL;
    delete candidate->m_curve;
    candidate->m_curve = profile.release();
    candidate->m_bbox = ON_BoundingBox::EmptyBoundingBox;
    candidate->BoundingBox();
    if (!candidate->IsValid())
	return NULL;
    return candidate.release();
}

} // namespace

#define CLASSNAME "OffsetSurface"
#define ENTITYNAME "Offset_Surface"
string OffsetSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)OffsetSurface::Create);

OffsetSurface::OffsetSurface()
{
    step = NULL;
    id = 0;
    basis_surface = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetSurface::OffsetSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    basis_surface = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetSurface::~OffsetSurface()
{
}

bool
OffsetSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!Surface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (basis_surface == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "basis_surface");
	if (entity) {
	    basis_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'basis_surface' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    distance = step->getRealAttribute(sse, "distance");
    self_intersect = step->getLogicalAttribute(sse, "self_intersect");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
OffsetSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    basis_surface->Print(level + 1);

    TAB(level + 1);
    std::cout << "distance:" << distance << std::endl;
    TAB(level + 1);
    std::cout << "self_intersect:" << step->getLogicalString((Logical)self_intersect) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Surface::Print(level + 1);
}

STEPEntity *
OffsetSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new OffsetSurface(sw, id);
}

STEPEntity *
OffsetSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
OffsetSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !basis_surface)
	return false;
    if (GetONId() >= 0)
	return true;
    if (trim_curve_3d_bbox)
	basis_surface->SetCurveBounds(trim_curve_3d_bbox);
    if (!basis_surface->LoadONBrep(brep))
	return false;
    const int basis_id = basis_surface->GetONId();
    if (basis_id < 0 || basis_id >= brep->m_S.Count() || !brep->m_S[basis_id])
	return false;

    if (!(LocalUnits::tolerance > 0.0))
	return false;
    const double tolerance = LocalUnits::tolerance;
    double maximum_deviation = 0.0;
    const double local_distance = distance * LocalUnits::length;
    size_t offset_profile_segments = 0;
    std::string specialized_failure;
    std::unique_ptr<ON_Surface> offset;
    const ON_SumSurface *sum =
	ON_SumSurface::Cast(brep->m_S[basis_id]);
    if (sum)
	offset.reset(offset_linear_extrusion(sum, local_distance, tolerance,
	    &maximum_deviation, &offset_profile_segments,
	    &specialized_failure));
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(brep->m_S[basis_id]);
    if (!offset && revolution)
	offset.reset(offset_surface_of_revolution(revolution, local_distance,
	    tolerance, &maximum_deviation, &offset_profile_segments,
	    &specialized_failure));
    if (!offset && revolution && step && step->Verbose() &&
	    !specialized_failure.empty())
	std::cerr << "OFFSET_SURFACE #" << id
	    << ": specialized revolution representation rejected: "
	    << specialized_failure << std::endl;
    if (!offset) {
	std::unique_ptr<ON_NurbsSurface> nurbs(ON_NurbsSurface::New());
	if (!nurbs || !brep->m_S[basis_id]->GetNurbForm(*nurbs))
	    return false;
	offset.reset(nurbs->Offset(local_distance, tolerance,
	    &maximum_deviation));
    }
    if (!offset || !offset->IsValid() || !std::isfinite(maximum_deviation) ||
	    maximum_deviation > tolerance)
	return false;
    SetONId(brep->AddSurface(offset.release()));
    if (GetONId() >= 0 && offset_profile_segments && step) {
	step->RecordRepair(id, "OFFSET_SURFACE", "basis_surface",
	    "represented an offset swept surface by a tolerance-bounded "
	    "profile within the declared STEP uncertainty");
	if (step->Verbose())
	    std::cerr << "OFFSET_SURFACE #" << id << ": represented the "
		<< (sum ? "offset extrusion" : "offset revolution")
		<< " with " << offset_profile_segments
		<< " profile segments; maximum deviation="
		<< maximum_deviation << " mm, tolerance=" << tolerance
		<< " mm" << std::endl;
    }
    return GetONId() >= 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
