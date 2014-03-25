/*                 NextAssemblyUsageOccurrence.cpp
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
/** @file step/NextAssemblyUsageOccurrence.cpp
 *
 * Routines to convert STEP "NextAssemblyUsageOccurrence" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinition.h"
#include "NextAssemblyUsageOccurrence.h"

#define CLASSNAME "NextAssemblyUsageOccurrence"
#define ENTITYNAME "Next_Assembly_Usage_Occurrence"
string NextAssemblyUsageOccurrence::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) NextAssemblyUsageOccurrence::Create);

NextAssemblyUsageOccurrence::NextAssemblyUsageOccurrence()
{
    step = NULL;
    id = 0;
}

NextAssemblyUsageOccurrence::NextAssemblyUsageOccurrence(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

NextAssemblyUsageOccurrence::~NextAssemblyUsageOccurrence()
{
}

ProductDefinition *
NextAssemblyUsageOccurrence::GetRelatingProductDefinition()
{
    return relating_product_definition;
}

ProductDefinition *
NextAssemblyUsageOccurrence::GetRelatedProductDefinition()
{
    return related_product_definition;
}

bool NextAssemblyUsageOccurrence::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!AssemblyComponentUsage::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	return false;
    }

    return true;
}

void NextAssemblyUsageOccurrence::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "ident:" << ident << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;

    TAB(level + 1);
    std::cout << "relating_product_definition:" << std::endl;
    relating_product_definition->Print(level + 1);
    TAB(level + 1);
    std::cout << "related_product_definition:" << std::endl;
    related_product_definition->Print(level + 2);
}

STEPEntity *
NextAssemblyUsageOccurrence::GetInstance(STEPWrapper *sw, int id)
{
    return new NextAssemblyUsageOccurrence(sw, id);
}

STEPEntity *
NextAssemblyUsageOccurrence::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool NextAssemblyUsageOccurrence::LoadONBrep(ON_Brep *brep)
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
