/*                 OffsetCurve3D.cpp
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
/** @file step/OffsetCurve3D.cpp
 *
 * Routines to convert STEP "OffsetCurve3D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator.h"

#include "OffsetCurve3D.h"
#include "LocalUnits.h"

#include <algorithm>
#include <cmath>
#include <functional>

#define CLASSNAME "OffsetCurve3D"
#define ENTITYNAME "Offset_Curve_3d"
string OffsetCurve3D::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)OffsetCurve3D::Create);

OffsetCurve3D::OffsetCurve3D()
{
    step = NULL;
    id = 0;
    basis_curve = NULL;
    distance = 0.0;
    self_intersect = LUnset;
    ref_direction = NULL;
}

OffsetCurve3D::OffsetCurve3D(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    basis_curve = NULL;
    distance = 0.0;
    self_intersect = LUnset;
    ref_direction = NULL;
}

OffsetCurve3D::~OffsetCurve3D()
{
    basis_curve = NULL;
    ref_direction = NULL;
}

bool
OffsetCurve3D::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Curve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (basis_curve == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "basis_curve");
	if (entity) {
	    basis_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw, entity)); //CreateCurveObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'basis_curve'." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    distance = step->getRealAttribute(sse, "distance");
    self_intersect = step->getLogicalAttribute(sse, "self_intersect");

    if (ref_direction == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "ref_direction");
	if (entity) {
	    ref_direction = dynamic_cast<Direction *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'ref_direction'." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

const double *
OffsetCurve3D::PointAtEnd()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
OffsetCurve3D::PointAtStart()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
OffsetCurve3D::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    basis_curve->Print(level + 1);
    TAB(level + 1);
    std::cout << "distance:" << distance << std::endl;
    TAB(level + 1);
    std::cout << "self_intersect:" << step->getLogicalString(self_intersect) << std::endl;
    ref_direction->Print(level + 1);
}

STEPEntity *
OffsetCurve3D::GetInstance(STEPWrapper *sw, int id)
{
    return new OffsetCurve3D(sw, id);
}

STEPEntity *
OffsetCurve3D::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
OffsetCurve3D::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !basis_curve || !ref_direction)
	return false;
    if (GetONId() >= 0)
	return true;
    if (trimmed) {
	if (parameter_trim)
	    basis_curve->SetParameterTrim(t, s);
	else
	    basis_curve->SetPointTrim(trim_startpoint, trim_endpoint);
    } else {
	basis_curve->Start(start);
	basis_curve->End(end);
    }
    if (!basis_curve->LoadONBrep(brep))
	return false;
    const int basis_id = basis_curve->GetONId();
    const ON_Curve *basis = basis_id >= 0 && basis_id < brep->m_C3.Count() ?
	brep->m_C3[basis_id] : NULL;
    if (!basis)
	return false;

    const double *ratios = ref_direction->DirectionRatios();
    ON_3dVector reference(ratios[0], ratios[1], ratios[2]);
    if (!reference.Unitize())
	return false;
    const double offset_distance = distance * LocalUnits::length;
    const double tolerance = std::max(LocalUnits::tolerance, 1.0e-9);
    const ON_Interval domain = basis->Domain();
    if (!domain.IsIncreasing())
	return false;

    const auto evaluate = [&](double parameter, ON_3dPoint *point) {
	ON_3dPoint basis_point;
	ON_3dVector tangent;
	if (!point || !basis->EvTangent(parameter, basis_point, tangent))
	    return false;
	ON_3dVector normal = ON_CrossProduct(tangent, reference);
	if (!normal.Unitize())
	    return false;
	*point = basis_point + offset_distance * normal;
	return point->IsValid();
    };
    const auto segment_distance = [](const ON_3dPoint &point,
	    const ON_3dPoint &start_point, const ON_3dPoint &end_point) {
	const ON_3dVector chord = end_point - start_point;
	const double squared_length = chord * chord;
	if (!(squared_length > ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE))
	    return point.DistanceTo(start_point);
	double fraction = ((point - start_point) * chord) / squared_length;
	fraction = std::max(0.0, std::min(1.0, fraction));
	return point.DistanceTo(start_point + fraction * chord);
    };

    ON_3dPoint first, last;
    if (!evaluate(domain.Min(), &first) || !evaluate(domain.Max(), &last))
	return false;
    ON_3dPointArray vertices;
    vertices.Append(first);
    const size_t maximum_vertices = 65536;
    std::function<bool(double, const ON_3dPoint &, double,
	const ON_3dPoint &, unsigned int)> refine;
    refine = [&](double a, const ON_3dPoint &pa, double b,
	    const ON_3dPoint &pb, unsigned int depth) {
	const double parameters[3] = {
	    a + 0.25 * (b - a), a + 0.5 * (b - a), a + 0.75 * (b - a)};
	ON_3dPoint samples[3];
	double deviation = 0.0;
	for (int i = 0; i < 3; ++i) {
	    if (!evaluate(parameters[i], &samples[i]))
		return false;
	    deviation = std::max(deviation,
		segment_distance(samples[i], pa, pb));
	}
	if (deviation <= tolerance) {
	    if (vertices.Count() >= static_cast<int>(maximum_vertices))
		return false;
	    vertices.Append(pb);
	    return true;
	}
	if (depth >= 24)
	    return false;
	return refine(a, pa, parameters[1], samples[1], depth + 1) &&
	    refine(parameters[1], samples[1], b, pb, depth + 1);
    };
    if (!refine(domain.Min(), first, domain.Max(), last, 0))
	return false;

    ON_PolylineCurve *offset = new ON_PolylineCurve(vertices);
    if (!offset->IsValid()) {
	delete offset;
	return false;
    }
    SetONId(brep->AddEdgeCurve(offset));
    if (step)
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    id, "OFFSET_CURVE_3D", "distance",
	    "constructed an adaptive offset polyline within the model tolerance");
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
