/*                 ProductDefinitionShape.cpp
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
/** @file step/ProductDefinitionShape.cpp
 *
 * Routines to convert STEP "ProductDefinitionShape" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CharacterizedDefinition.h"
#include "ProductDefinitionShape.h"
#include "ProductDefinition.h"

#define CLASSNAME "ProductDefinitionShape"
#define ENTITYNAME "Product_Definition_Shape"
string ProductDefinitionShape::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionShape::Create);

ProductDefinitionShape::ProductDefinitionShape()
{
    step = NULL;
    id = 0;
    definition = NULL;
}

ProductDefinitionShape::ProductDefinitionShape(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    definition = NULL;
}

ProductDefinitionShape::~ProductDefinitionShape()
{
}

string ProductDefinitionShape::ClassName()
{
    return entityname;
}

string ProductDefinitionShape::GetProductName()
{
    string pname = "";

    pname = definition->GetProductName();

    return pname;
}

int
ProductDefinitionShape::GetProductId()
{
    int ret = 0;

    if (definition) {
	ret = definition->GetProductId();
    }

    return ret;
}

bool ProductDefinitionShape::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!PropertyDefinition::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::PropertyDefinition." << std::endl;
	return false;
    }

    return true;
}

void ProductDefinitionShape::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    PropertyDefinition::Print(level + 2);
}

STEPEntity *
ProductDefinitionShape::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionShape(sw, id);
}

STEPEntity *
ProductDefinitionShape::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionShape::LoadONBrep(ON_Brep *brep)
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
