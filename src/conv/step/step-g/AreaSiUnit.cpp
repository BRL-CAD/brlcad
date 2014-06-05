/*                 AreaSiUnit.cpp
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
/** @file step/AreaSiUnit.cpp
 *
 * Routines to convert STEP "AreaSiUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "AreaSiUnit.h"

#define CLASSNAME "AreaSiUnit"
#define ENTITYNAME "Area_Si_Unit"
string AreaSiUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)AreaSiUnit::Create);

AreaSiUnit::AreaSiUnit()
{
    step = NULL;
    id = 0;
}

AreaSiUnit::AreaSiUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

AreaSiUnit::~AreaSiUnit()
{
}

bool
AreaSiUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!AreaUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }
    if (!SiUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }

    return true;
}

void
AreaSiUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    AreaUnit::Print(level + 1);
    SiUnit::Print(level + 1);

}

STEPEntity *
AreaSiUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new AreaSiUnit(sw, id);
}

STEPEntity *
AreaSiUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
