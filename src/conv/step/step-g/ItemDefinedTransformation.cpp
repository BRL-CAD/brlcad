/*                 ItemDefinedTransformation.cpp
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
/** @file step/ItemDefinedTransformation.cpp
 *
 * Routines to convert STEP "ItemDefinedTransformation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RepresentationItem.h"
#include "ItemDefinedTransformation.h"

#define CLASSNAME "ItemDefinedTransformation"
#define ENTITYNAME "Item_Defined_Transformation"
string ItemDefinedTransformation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ItemDefinedTransformation::Create);

ItemDefinedTransformation::ItemDefinedTransformation()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
    transform_item_1 = NULL;
    transform_item_2 = NULL;
}

ItemDefinedTransformation::ItemDefinedTransformation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
    transform_item_1 = NULL;
    transform_item_2 = NULL;
}

ItemDefinedTransformation::~ItemDefinedTransformation()
{
}

string ItemDefinedTransformation::ClassName()
{
    return entityname;
}

bool ItemDefinedTransformation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Transformation::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Transformation." << std::endl;
	return false;
    }
    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    if (transform_item_1 == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "transform_item_1");
	if (entity) { //this attribute is optional
	    transform_item_1 = dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'transform_item_1'." << std::endl;
	    return false;
	}
    }
    if (transform_item_2 == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "transform_item_2");
	if (entity) { //this attribute is optional
	    transform_item_2 = dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'transform_item_2'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ItemDefinedTransformation::Print(int level)
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
    TAB(level + 1);
    std::cout << "transform_item_1:" << name << std::endl;
    transform_item_1->Print(level + 2);
    TAB(level + 1);
    std::cout << "transform_item_2:" << name << std::endl;
    transform_item_2->Print(level + 2);
}

STEPEntity *
ItemDefinedTransformation::GetInstance(STEPWrapper *sw, int id)
{
    return new ItemDefinedTransformation(sw, id);
}

STEPEntity *
ItemDefinedTransformation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ItemDefinedTransformation::LoadONBrep(ON_Brep *brep)
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
