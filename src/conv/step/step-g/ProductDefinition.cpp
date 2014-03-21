/*                 ProductDefinition.cpp
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
/** @file step/ProductDefinition.cpp
 *
 * Routines to convert STEP "ProductDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinitionFormation.h"
#include "ProductDefinitionFormationWithSpecifiedSource.h"
#include "ProductDefinitionContext.h"
#include "ProductDefinition.h"

#define CLASSNAME "ProductDefinition"
#define ENTITYNAME "Product_Definition"
string ProductDefinition::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinition::Create);

ProductDefinition::ProductDefinition()
{
    step = NULL;
    id = 0;
    ident = "";
    description = "";
    formation = NULL;
    frame_of_reference = NULL;
}

ProductDefinition::ProductDefinition(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    ident = "";
    description = "";
    formation = NULL;
    frame_of_reference = NULL;
}

ProductDefinition::~ProductDefinition()
{
    // created through factory will be deleted there.
    formation = NULL;
    frame_of_reference = NULL;
}

string ProductDefinition::ClassName()
{
    return entityname;
}

string ProductDefinition::Ident()
{
    return ident;
}

string ProductDefinition::Description()
{
    return description;
}

string ProductDefinition::GetProductName()
{
    string name;
    ProductDefinitionFormation *aPDF = dynamic_cast<ProductDefinitionFormation *>(formation);
    if (aPDF != NULL) {
	name = aPDF->GetProductName();
    }
    return name;
}

int
ProductDefinition::GetProductId()
{
    int ret = 0;
    ProductDefinitionFormation *aPDF = dynamic_cast<ProductDefinitionFormation *>(formation);
    if (aPDF != NULL) {
	ret = aPDF->GetProductId();
    }
    return ret;
}

bool ProductDefinition::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    ident = step->getStringAttribute(sse, "id");
    description = step->getStringAttribute(sse, "description");

    if (formation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "formation");
	if (entity) { //this attribute is optional
	    formation = dynamic_cast<ProductDefinitionFormation *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'formation'." << std::endl;
	    return false;
	}
    }

    if (frame_of_reference == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "frame_of_reference");
	if (entity) { //this attribute is optional
	    frame_of_reference = dynamic_cast<ProductDefinitionContext *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'frame_of_reference'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ProductDefinition::Print(int level)
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
    std::cout << "formation:" << std::endl;
    formation->Print(level + 1);
    TAB(level + 1);
    std::cout << "frame_of_reference:" << std::endl;
    frame_of_reference->Print(level + 2);
}

STEPEntity *
ProductDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinition(sw, id);
}

STEPEntity *
ProductDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinition::LoadONBrep(ON_Brep *brep)
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
