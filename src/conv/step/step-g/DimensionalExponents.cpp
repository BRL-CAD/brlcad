/*        D I M E N S I O N A L E X P O N E N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file DimensionalExponents.cpp
 *
 * Routines to convert STEP "DimensionalExponents" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "DimensionalExponents.h"

#define CLASSNAME "DimensionalExponents"
#define ENTITYNAME "Dimensional_Exponents"
string DimensionalExponents::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)DimensionalExponents::Create);

DimensionalExponents::DimensionalExponents()
{
    step = NULL;
    id = 0;
    length_exponent = 0.0;
    mass_exponent = 0.0;
    time_exponent = 0.0;
    electric_current_exponent = 0.0;
    thermodynamic_temperature_exponent = 0.0;
    amount_of_substance_exponent = 0.0;
    luminous_intensity_exponent = 0.0;
}

DimensionalExponents::DimensionalExponents(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    length_exponent = 0.0;
    mass_exponent = 0.0;
    time_exponent = 0.0;
    electric_current_exponent = 0.0;
    thermodynamic_temperature_exponent = 0.0;
    amount_of_substance_exponent = 0.0;
    luminous_intensity_exponent = 0.0;
}

DimensionalExponents::~DimensionalExponents()
{
}

bool
DimensionalExponents::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    length_exponent = step->getRealAttribute(sse, "length_exponent");
    mass_exponent = step->getRealAttribute(sse, "mass_exponent");
    time_exponent = step->getRealAttribute(sse, "time_exponent");
    electric_current_exponent = step->getRealAttribute(sse, "electric_current_exponent");
    thermodynamic_temperature_exponent = step->getRealAttribute(sse, "thermodynamic_temperature_exponent");
    amount_of_substance_exponent = step->getRealAttribute(sse, "amount_of_substance_exponent");
    luminous_intensity_exponent = step->getRealAttribute(sse, "luminous_intensity_exponent");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
DimensionalExponents::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Local Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "length_exponent:" << length_exponent << std::endl;
    TAB(level + 1);
    std::cout << "mass_exponent:" << mass_exponent << std::endl;
    TAB(level + 1);
    std::cout << "time_exponent:" << time_exponent << std::endl;
    TAB(level + 1);
    std::cout << "electric_current_exponent:" << electric_current_exponent << std::endl;
    TAB(level + 1);
    std::cout << "thermodynamic_temperature_exponent:" << thermodynamic_temperature_exponent << std::endl;
    TAB(level + 1);
    std::cout << "amount_of_substance_exponent:" << amount_of_substance_exponent << std::endl;
    TAB(level + 1);
    std::cout << "luminous_intensity_exponent:" << luminous_intensity_exponent << std::endl;
}

STEPEntity *
DimensionalExponents::GetInstance(STEPWrapper *sw, int id)
{
    return new DimensionalExponents(sw, id);
}

STEPEntity *
DimensionalExponents::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
