/*                 Ellipse.cpp
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

#include "Vertex.h"
#include "Ellipse.h"

#define CLASSNAME "Ellipse"
#define ENTITYNAME "Ellipse"
string Ellipse::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Ellipse::Create);

Ellipse::Ellipse()
{
    step = NULL;
    id = 0;
    semi_axis_1 = 0.0;
    semi_axis_2 = 0.0;
}

Ellipse::Ellipse(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    semi_axis_1 = 0.0;
    semi_axis_2 = 0.0;
}

Ellipse::~Ellipse()
{
}

bool
Ellipse::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
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

    semi_axis_1 = step->getRealAttribute(sse, "semi_axis_1");
    semi_axis_2 = step->getRealAttribute(sse, "semi_axis_2");

    return true;
}

void
Ellipse::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "semi_axis_1:" << semi_axis_1 << std::endl;
    TAB(level + 1);
    std::cout << "semi_axis_2:" << semi_axis_2 << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Conic::Print(level);
}

STEPEntity *
Ellipse::GetInstance(STEPWrapper *sw, int id)
{
    return new Ellipse(sw, id);
}

STEPEntity *
Ellipse::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
