/*                 RationalBezierCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2016 United States Government as represented by
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
/** @file step/RationalBezierCurve.cpp
 *
 * Routines to convert STEP "RationalBezierCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalBezierCurve.h"

#define CLASSNAME "RationalBezierCurve"
#define ENTITYNAME "Rational_Bezier_Curve"
string RationalBezierCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RationalBezierCurve::Create);

RationalBezierCurve::RationalBezierCurve()
{
    step = NULL;
    id = 0;
}

RationalBezierCurve::RationalBezierCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RationalBezierCurve::~RationalBezierCurve()
{
}

bool
RationalBezierCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RationalBSplineCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RationalBSplineCurve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
RationalBezierCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    RationalBSplineCurve::Print(level);

}

STEPEntity *
RationalBezierCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new RationalBezierCurve(sw, id);
}

STEPEntity *
RationalBezierCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
