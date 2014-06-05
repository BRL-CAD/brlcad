/*                 PlaneAngleContextDependentUnit.cpp
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
/** @file step/PlaneAngleContextDependentUnit.cpp
 *
 * Routines to convert STEP "PlaneAngleContextDependentUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "PlaneAngleContextDependentUnit.h"

#define CLASSNAME "PlaneAngleContextDependentUnit"
#define ENTITYNAME "Plane_Angle_Context_Dependent_Unit"
string PlaneAngleContextDependentUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)PlaneAngleContextDependentUnit::Create);

PlaneAngleContextDependentUnit::PlaneAngleContextDependentUnit()
{
    step = NULL;
    id = 0;
}

PlaneAngleContextDependentUnit::PlaneAngleContextDependentUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

PlaneAngleContextDependentUnit::~PlaneAngleContextDependentUnit()
{
}

bool
PlaneAngleContextDependentUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!PlaneAngleUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }
    if (!ContextDependentUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }

    return true;
}

void
PlaneAngleContextDependentUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    PlaneAngleUnit::Print(level + 1);
    ContextDependentUnit::Print(level + 1);

}

STEPEntity *
PlaneAngleContextDependentUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new PlaneAngleContextDependentUnit(sw, id);
}

STEPEntity *
PlaneAngleContextDependentUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
