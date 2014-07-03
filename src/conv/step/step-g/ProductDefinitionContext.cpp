/*                 ProductDefinitionContext.cpp
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
/** @file step/ProductDefinitionContext.cpp
 *
 * Routines to convert STEP "ProductDefinitionContext" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinitionContext.h"

#define CLASSNAME "ProductDefinitionContext"
#define ENTITYNAME "Product_Definition_Context"
string ProductDefinitionContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionContext::Create);

ProductDefinitionContext::ProductDefinitionContext()
{
    step = NULL;
    id = 0;
    life_cycle_stage = "";
}

ProductDefinitionContext::ProductDefinitionContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    life_cycle_stage = "";
}

ProductDefinitionContext::~ProductDefinitionContext()
{
}

bool ProductDefinitionContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ApplicationContextElement::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ApplicationContextElement." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    life_cycle_stage = step->getStringAttribute(sse, "life_cycle_stage");

    return true;
}

void ProductDefinitionContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "life_cycle_stage:" << life_cycle_stage << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ApplicationContextElement::Print(level + 1);
}

STEPEntity *
ProductDefinitionContext::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionContext(sw, id);
}

STEPEntity *
ProductDefinitionContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionContext::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
