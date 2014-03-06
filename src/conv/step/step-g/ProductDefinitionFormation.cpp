/*                 ProductDefinitionFormation.cpp
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
/** @file step/ProductDefinitionFormation.cpp
 *
 * Routines to convert STEP "ProductDefinitionFormation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Product.h"
#include "ProductDefinitionFormation.h"

#define CLASSNAME "ProductDefinitionFormation"
#define ENTITYNAME "Product_Definition_Formation"
string ProductDefinitionFormation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionFormation::Create);

ProductDefinitionFormation::ProductDefinitionFormation()
{
    step = NULL;
    id = 0;
    ident = "";
    description = "";
    of_product = NULL;
}

ProductDefinitionFormation::ProductDefinitionFormation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    ident = "";
    description = "";
    of_product = NULL;
}

ProductDefinitionFormation::~ProductDefinitionFormation()
{
    // created through factory will be deleted there.
    of_product = NULL;
}

string ProductDefinitionFormation::ClassName()
{
    return entityname;
}

string ProductDefinitionFormation::Ident()
{
    return ident;
}

string ProductDefinitionFormation::Description()
{
    return description;
}

string ProductDefinitionFormation::GetProductName()
{
    return of_product->Name();
}

bool ProductDefinitionFormation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    ident = step->getStringAttribute(sse, "id");
    description = step->getStringAttribute(sse, "description");

    if (of_product == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "of_product");
	if (entity) { //this attribute is optional
	    of_product = dynamic_cast<Product *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'of_product'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ProductDefinitionFormation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "ident:" << ident << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;

    TAB(level + 1);
    std::cout << "of_product:" << std::endl;
    of_product->Print(level + 1);
}

STEPEntity *
ProductDefinitionFormation::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionFormation(sw, id);
}

STEPEntity *
ProductDefinitionFormation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionFormation::LoadONBrep(ON_Brep *brep)
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
