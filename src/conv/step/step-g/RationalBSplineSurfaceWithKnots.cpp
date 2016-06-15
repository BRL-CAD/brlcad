/*                 RationalBSplineSurfaceWithKnots.cpp
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
/** @file step/RationalBSplineSurfaceWithKnots.cpp
 *
 * Routines to convert STEP "RationalBSplineSurfaceWithKnots" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalBSplineSurfaceWithKnots.h"

#define CLASSNAME "RationalBSplineSurfaceWithKnots"
#define ENTITYNAME "Rational_B_Spline_Surface_With_Knots"
string RationalBSplineSurfaceWithKnots::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RationalBSplineSurfaceWithKnots::Create);

RationalBSplineSurfaceWithKnots::RationalBSplineSurfaceWithKnots()
{
    step = NULL;
    id = 0;
}

RationalBSplineSurfaceWithKnots::RationalBSplineSurfaceWithKnots(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RationalBSplineSurfaceWithKnots::~RationalBSplineSurfaceWithKnots()
{
}

bool
RationalBSplineSurfaceWithKnots::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RationalBSplineSurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RationalBSplineSurface." << std::endl;
	return false;
    }
    if (!BSplineSurfaceWithKnots::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BSplineSurfaceWithKnots." << std::endl;
	return false;
    }

    return true;
}

void
RationalBSplineSurfaceWithKnots::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    RationalBSplineSurface::Print(level);
    BSplineSurfaceWithKnots::Print(level);
}

STEPEntity *
RationalBSplineSurfaceWithKnots::GetInstance(STEPWrapper *sw, int id)
{
    return new RationalBSplineSurfaceWithKnots(sw, id);
}

STEPEntity *
RationalBSplineSurfaceWithKnots::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
