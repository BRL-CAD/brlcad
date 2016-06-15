/*                 ProductContext.cpp
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
/** @file step/ProductContext.cpp
 *
 * Routines to convert STEP "ProductContext" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductContext.h"

#define CLASSNAME "ProductContext"
#define ENTITYNAME "Product_Context"
string ProductContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductContext::Create);

ProductContext::ProductContext()
{
    step = NULL;
    id = 0;
    discipline_type = "";
}

ProductContext::ProductContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    discipline_type = "";
}

ProductContext::~ProductContext()
{
}

bool ProductContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
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

    discipline_type = step->getStringAttribute(sse, "discipline_type");

    return true;
}

void ProductContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "discipline_type:" << discipline_type << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ApplicationContextElement::Print(level + 1);
}

STEPEntity *
ProductContext::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductContext(sw, id);
}

STEPEntity *
ProductContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductContext::LoadONBrep(ON_Brep *brep)
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
