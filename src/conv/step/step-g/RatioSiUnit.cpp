/*                 RatioSiUnit.cpp
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
/** @file step/RatioSiUnit.cpp
 *
 * Routines to convert STEP "RatioSiUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RatioSiUnit.h"

#define CLASSNAME "RatioSiUnit"
#define ENTITYNAME "Ratio_Si_Unit"
string RatioSiUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RatioSiUnit::Create);

RatioSiUnit::RatioSiUnit()
{
    step = NULL;
    id = 0;
}

RatioSiUnit::RatioSiUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RatioSiUnit::~RatioSiUnit()
{
}

bool
RatioSiUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!RatioUnit::Load(step, sse)) {
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
RatioSiUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RatioUnit::Print(level + 1);
    SiUnit::Print(level + 1);

}

STEPEntity *
RatioSiUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new RatioSiUnit(sw, id);
}

STEPEntity *
RatioSiUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
