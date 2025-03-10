/*                 RationalBSplineCurveWithKnots.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2025 United States Government as represented by
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
/** @file step/RationalBSplineCurveWithKnots.cpp
 *
 * Routines to convert STEP "RationalBSplineCurveWithKnots" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalBSplineCurveWithKnots.h"

#define CLASSNAME "RationalBSplineCurveWithKnots"
#define ENTITYNAME "Rational_B_Spline_Curve_With_Knots"
string RationalBSplineCurveWithKnots::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RationalBSplineCurveWithKnots::Create);

RationalBSplineCurveWithKnots::RationalBSplineCurveWithKnots()
{
    step = NULL;
    id = 0;
}

RationalBSplineCurveWithKnots::RationalBSplineCurveWithKnots(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RationalBSplineCurveWithKnots::~RationalBSplineCurveWithKnots()
{
}

bool
RationalBSplineCurveWithKnots::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RationalBSplineCurve::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RationalBSplineCurve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    if (!BSplineCurveWithKnots::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BSplineCurveWithKnots." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
RationalBSplineCurveWithKnots::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;
    std::cout << "ON_id:(" << ON_id << ")" << std::endl;

    RationalBSplineCurve::Print(level + 1);
    BSplineCurveWithKnots::Print(level + 1);
}

STEPEntity *
RationalBSplineCurveWithKnots::GetInstance(STEPWrapper *sw, int id)
{
    return new RationalBSplineCurveWithKnots(sw, id);
}

STEPEntity *
RationalBSplineCurveWithKnots::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
