/*                 UncertaintyMeasureWithUnit.cpp
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
/** @file step/UncertaintyMeasureWithUnit.cpp
 *
 * Routines to convert STEP "UncertaintyMeasureWithUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "UncertaintyMeasureWithUnit.h"

#define CLASSNAME "UncertaintyMeasureWithUnit"
#define ENTITYNAME "Uncertainty_Measure_With_Unit"
string UncertaintyMeasureWithUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)UncertaintyMeasureWithUnit::Create);

UncertaintyMeasureWithUnit::UncertaintyMeasureWithUnit()
{
    step = NULL;
    id = 0;
}

UncertaintyMeasureWithUnit::UncertaintyMeasureWithUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

UncertaintyMeasureWithUnit::~UncertaintyMeasureWithUnit()
{
}

bool
UncertaintyMeasureWithUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!MeasureWithUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::MeasureWithUnit." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
UncertaintyMeasureWithUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;


    TAB(level);
    std::cout << "Local Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;
    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    MeasureWithUnit::Print(level + 1);
}

STEPEntity *
UncertaintyMeasureWithUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new UncertaintyMeasureWithUnit(sw, id);
}

STEPEntity *
UncertaintyMeasureWithUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
