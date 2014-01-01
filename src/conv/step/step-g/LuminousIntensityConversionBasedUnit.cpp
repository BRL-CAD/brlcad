/*                 LuminousIntensityConversionBasedUnit.cpp
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
/** @file step/LuminousIntensityConversionBasedUnit.cpp
 *
 * Routines to convert STEP "LuminousIntensityConversionBasedUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "MeasureWithUnit.h"
#include "LuminousIntensityConversionBasedUnit.h"

#define CLASSNAME "LuminousIntensityConversionBasedUnit"
#define ENTITYNAME "Luminous_Intensity_Conversion_Based_Unit"
string LuminousIntensityConversionBasedUnit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)LuminousIntensityConversionBasedUnit::Create);

LuminousIntensityConversionBasedUnit::LuminousIntensityConversionBasedUnit()
{
    step = NULL;
    id = 0;
}

LuminousIntensityConversionBasedUnit::LuminousIntensityConversionBasedUnit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

LuminousIntensityConversionBasedUnit::~LuminousIntensityConversionBasedUnit()
{
}

bool
LuminousIntensityConversionBasedUnit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!LuminousIntensityUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }
    if (!ConversionBasedUnit::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
	return false;
    }

    return true;
}

void
LuminousIntensityConversionBasedUnit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    LuminousIntensityUnit::Print(level + 1);
    ConversionBasedUnit::Print(level + 1);

}

STEPEntity *
LuminousIntensityConversionBasedUnit::GetInstance(STEPWrapper *sw, int id)
{
    return new LuminousIntensityConversionBasedUnit(sw, id);
}

STEPEntity *
LuminousIntensityConversionBasedUnit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
