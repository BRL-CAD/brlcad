/*                 AssemblyComponentUsage.cpp
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
/** @file step/AssemblyComponentUsage.cpp
 *
 * Routines to convert STEP "AssemblyComponentUsage" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "AssemblyComponentUsage.h"

#define CLASSNAME "AssemblyComponentUsage"
#define ENTITYNAME "Assembly_Component_Usage"
string AssemblyComponentUsage::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) AssemblyComponentUsage::Create);

AssemblyComponentUsage::AssemblyComponentUsage()
{
    step = NULL;
    id = 0;
    reference_designator = "";
}

AssemblyComponentUsage::AssemblyComponentUsage(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    reference_designator = "";
}

AssemblyComponentUsage::~AssemblyComponentUsage()
{
}

bool
AssemblyComponentUsage::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ProductDefinitionUsage::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    reference_designator = step->getStringAttribute(sse, "reference_designator");
    return true;
}

void AssemblyComponentUsage::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;

    ProductDefinitionUsage::Print(level + 1);

    TAB(level + 1);
    if (!reference_designator.empty()) {
	std::cout << "OPTIONAL reference_designator: " << reference_designator << std::endl;
    }
}

STEPEntity *
AssemblyComponentUsage::GetInstance(STEPWrapper *sw, int id)
{
    return new AssemblyComponentUsage(sw, id);
}

STEPEntity *
AssemblyComponentUsage::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool AssemblyComponentUsage::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
