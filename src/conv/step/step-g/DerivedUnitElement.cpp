/*          D E R I V E D U N I T E L E M E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file DerivedUnitElement.cpp
 *
 * Routines to convert STEP "DerivedUnitElement" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "NamedUnit.h"
#include "DerivedUnitElement.h"

#define CLASSNAME "DerivedUnitElement"
#define ENTITYNAME "Derived_Unit_Element"
string DerivedUnitElement::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)DerivedUnitElement::Create);

DerivedUnitElement::DerivedUnitElement()
{
    step = NULL;
    id = 0;
    unit = NULL;
    exponent = 0.0;
}

DerivedUnitElement::DerivedUnitElement(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    unit = NULL;
    exponent = 0.0;
}

DerivedUnitElement::~DerivedUnitElement()
{
}

bool
DerivedUnitElement::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (unit == NULL) {
	SDAI_Application_instance *se = step->getEntityAttribute(sse, "unit");

	if (se != NULL) {
	    //unit = dynamic_cast<NamedUnit*>(Factory::CreateNamedUnitObject(sw,se));
	    unit = dynamic_cast<NamedUnit *>(Factory::CreateObject(sw, se));
	    if (unit == NULL) {
		std::cout << CLASSNAME << ":Error loading member field \"unit\"." << std::endl;
		return false;
	    }
	} else {
	    std::cout << CLASSNAME << ":Error loading member field \"unit\"." << std::endl;
	    return false;
	}
    }
    exponent = step->getRealAttribute(sse, "exponent");

    return true;
}

void
DerivedUnitElement::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "unit:" << std::endl;
    unit->Print(level + 1);
    TAB(level + 1);
    std::cout << "exponent:" << exponent << std::endl;
}

STEPEntity *
DerivedUnitElement::GetInstance(STEPWrapper *sw, int id)
{
    return new DerivedUnitElement(sw, id);
}

STEPEntity *
DerivedUnitElement::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
