/*                 Circle.cpp
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
/** @file step/Curve.cpp
 *
 * Routines to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Circle.h"

#define CLASSNAME "Circle"
#define ENTITYNAME "Circle"
string Circle::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Circle::Create);

Circle::Circle()
{
    step = NULL;
    id = 0;
    radius = 0.0;
}

Circle::Circle(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    radius = 0.0;
}

Circle::~Circle()
{
}

bool
Circle::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Conic::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Conic." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    radius = step->getRealAttribute(sse, "radius");

    return true;
}

void
Circle::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "radius:" << radius << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Conic::Print(level + 1);
}

STEPEntity *
Circle::GetInstance(STEPWrapper *sw, int id)
{
    return new Circle(sw, id);
}

STEPEntity *
Circle::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
