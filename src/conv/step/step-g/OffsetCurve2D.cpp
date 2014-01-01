/*                 OffsetCurve2D.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/OffsetCurve2D.cpp
 *
 * Routines to convert STEP "OffsetCurve2D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator.h"

#include "OffsetCurve2D.h"

#define CLASSNAME "OffsetCurve2D"
#define ENTITYNAME "Offset_Curve_2d"
string OffsetCurve2D::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)OffsetCurve2D::Create);

OffsetCurve2D::OffsetCurve2D()
{
    step = NULL;
    id = 0;
    basis_curve = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetCurve2D::OffsetCurve2D(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    basis_curve = NULL;
    distance = 0.0;
    self_intersect = LUnset;
}

OffsetCurve2D::~OffsetCurve2D()
{
    basis_curve = NULL;
}

bool
OffsetCurve2D::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Curve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
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
	    return false;
	}
    }

    distance = step->getRealAttribute(sse, "distance");
    self_intersect = step->getLogicalAttribute(sse, "self_intersect");

    return true;
}

const double *
OffsetCurve2D::PointAtEnd()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
OffsetCurve2D::PointAtStart()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
OffsetCurve2D::Print(int level)
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
}

STEPEntity *
OffsetCurve2D::GetInstance(STEPWrapper *sw, int id)
{
    return new OffsetCurve2D(sw, id);
}

STEPEntity *
OffsetCurve2D::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
