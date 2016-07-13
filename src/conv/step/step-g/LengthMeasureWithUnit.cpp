/*                 LengthMeasureWithUnit.cpp
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
/** @file step/LengthMeasureWithUnit.cpp
 *
 * Routines to convert STEP "LengthMeasureWithUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "LengthSiUnit.h"

#include "LengthMeasureWithUnit.h"

#define CLASSNAME "LengthMeasureWithUnit"
#define ENTITYNAME "Length_Measure_With_Unit"
string LengthMeasureWithUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)LengthMeasureWithUnit::Create);

LengthMeasureWithUnit::LengthMeasureWithUnit()
{
    step = NULL;
    id = 0;
}

LengthMeasureWithUnit::LengthMeasureWithUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

LengthMeasureWithUnit::~LengthMeasureWithUnit()
{
}

bool
LengthMeasureWithUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!MeasureWithUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::" << CLASSNAME << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
LengthMeasureWithUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;


    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    MeasureWithUnit::Print(level + 1);
}

STEPEntity *
LengthMeasureWithUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new LengthMeasureWithUnit(sw, id);
}

STEPEntity *
LengthMeasureWithUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
