/*                 ElectricCurrentConversionBasedUnit.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
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
/** @file step/ElectricCurrentConversionBasedUnit.cpp
 *
 * Routines to convert STEP "ElectricCurrentConversionBasedUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "MeasureWithUnit.h"
#include "ElectricCurrentConversionBasedUnit.h"

#define CLASSNAME "ElectricCurrentConversionBasedUnit"
#define ENTITYNAME "Electric_Current_Conversion_Based_Unit"
string ElectricCurrentConversionBasedUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ElectricCurrentConversionBasedUnit::Create);

ElectricCurrentConversionBasedUnit::ElectricCurrentConversionBasedUnit()
{
    step = NULL;
    id = 0;
}

ElectricCurrentConversionBasedUnit::ElectricCurrentConversionBasedUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ElectricCurrentConversionBasedUnit::~ElectricCurrentConversionBasedUnit()
{
}

bool
ElectricCurrentConversionBasedUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!ConversionBasedUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
ElectricCurrentConversionBasedUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ElectricCurrentUnit::Print(level + 1);
    ConversionBasedUnit::Print(level + 1);

}

STEPEntity *
ElectricCurrentConversionBasedUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new ElectricCurrentConversionBasedUnit(sw, id);
}

STEPEntity *
ElectricCurrentConversionBasedUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
