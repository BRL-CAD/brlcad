/*                 ProductDefinitionUsage.cpp
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
/** @file step/ProductDefinitionUsage.cpp
 *
 * Routines to convert STEP "ProductDefinitionUsage" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinitionUsage.h"

#define CLASSNAME "ProductDefinitionUsage"
#define ENTITYNAME "Product_Definition_Usage"
string ProductDefinitionUsage::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionUsage::Create);

ProductDefinitionUsage::ProductDefinitionUsage()
{
    step = NULL;
    id = 0;
}

ProductDefinitionUsage::ProductDefinitionUsage(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ProductDefinitionUsage::~ProductDefinitionUsage()
{
}

bool
ProductDefinitionUsage::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ProductDefinitionRelationship::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	return false;
    }

    return true;
}

void ProductDefinitionUsage::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;

    ProductDefinitionRelationship::Print(level + 1);
}

STEPEntity *
ProductDefinitionUsage::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionUsage(sw, id);
}

STEPEntity *
ProductDefinitionUsage::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionUsage::LoadONBrep(ON_Brep *brep)
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
