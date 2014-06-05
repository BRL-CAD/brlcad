/*                 ProductDefinitionRelationship.cpp
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
/** @file step/ProductDefinitionRelationship.cpp
 *
 * Routines to convert STEP "ProductDefinitionRelationship" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductDefinition.h"
#include "ProductDefinitionRelationship.h"

#define CLASSNAME "ProductDefinitionRelationship"
#define ENTITYNAME "Product_Definition_Relationship"
string ProductDefinitionRelationship::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductDefinitionRelationship::Create);

ProductDefinitionRelationship::ProductDefinitionRelationship()
{
    step = NULL;
    id = 0;
    ident = "";
    name = "";
    description = "";
    relating_product_definition = NULL;
    related_product_definition = NULL;
}

ProductDefinitionRelationship::ProductDefinitionRelationship(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    ident = "";
    name = "";
    description = "";
    relating_product_definition = NULL;
    related_product_definition = NULL;
}

ProductDefinitionRelationship::~ProductDefinitionRelationship()
{
    // created through factory will be deleted there.
    relating_product_definition = NULL;
    related_product_definition = NULL;
}

string ProductDefinitionRelationship::ClassName()
{
    return entityname;
}

string ProductDefinitionRelationship::Ident()
{
    return ident;
}

string ProductDefinitionRelationship::Description()
{
    return description;
}

string ProductDefinitionRelationship::GetName()
{
    return name;
}
ProductDefinition *
ProductDefinitionRelationship::GetRelatingProductDefinition()
{
    return relating_product_definition;
}
ProductDefinition *
ProductDefinitionRelationship::GetRelatedProductDefinition()
{
    return related_product_definition;
}

bool ProductDefinitionRelationship::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    ident = step->getStringAttribute(sse, "id");
    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    if (relating_product_definition == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "relating_product_definition");
	if (entity) { //this attribute is optional
	    relating_product_definition = dynamic_cast<ProductDefinition *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'relating_product_definition'." << std::endl;
	    return false;
	}
    }

    if (related_product_definition == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "related_product_definition");
	if (entity) { //this attribute is optional
	    related_product_definition = dynamic_cast<ProductDefinition *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'related_product_definition'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ProductDefinitionRelationship::Print(int level)
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
ProductDefinitionRelationship::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductDefinitionRelationship(sw, id);
}

STEPEntity *
ProductDefinitionRelationship::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductDefinitionRelationship::LoadONBrep(ON_Brep *brep)
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
