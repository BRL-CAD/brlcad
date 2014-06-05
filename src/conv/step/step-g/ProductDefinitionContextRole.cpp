/*                 ProductDefinitionContextRole.cpp
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
/** @file step/ProductDefinitionContextRole.cpp
 *
 * Routines to convert STEP "ProductDefinitionContextRole" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinitionContextRole.h"

#define CLASSNAME "ProductDefinitionContextRole"
#define ENTITYNAME "Product_Definition_Context_Role"
string ProductDefinitionContextRole::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionContextRole::Create);

ProductDefinitionContextRole::ProductDefinitionContextRole()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
}

ProductDefinitionContextRole::ProductDefinitionContextRole(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
}

ProductDefinitionContextRole::~ProductDefinitionContextRole()
{
}

string ProductDefinitionContextRole::ClassName()
{
    return entityname;
}

string ProductDefinitionContextRole::Name()
{
    return name;
}

string ProductDefinitionContextRole::Description()
{
    return description;
}

bool ProductDefinitionContextRole::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    return true;
}

void ProductDefinitionContextRole::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;
}

STEPEntity *
ProductDefinitionContextRole::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionContextRole(sw, id);
}

STEPEntity *
ProductDefinitionContextRole::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionContextRole::LoadONBrep(ON_Brep *brep)
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
