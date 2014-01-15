/*                 Product.cpp
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
/** @file step/Product.cpp
 *
 * Routines to convert STEP "Product" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ProductContext.h"
#include "Product.h"

#define CLASSNAME "Product"
#define ENTITYNAME "Product"
string Product::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) Product::Create);

Product::Product()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
}

Product::Product(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
}

Product::~Product()
{
}

string Product::ClassName()
{
    return entityname;
}

string Product::Name()
{
    return name;
}

string Product::Description()
{
    return description;
}

bool Product::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    if (frame_of_reference.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "frame_of_reference");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		ProductContext *aPC = dynamic_cast<ProductContext *>(Factory::CreateObject(sw, entity));

		frame_of_reference.push_back(aPC);
	    } else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'frame_of_reference'." << std::endl;
		l->clear();
		delete l;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    return true;
}

void Product::Print(int level)
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
    std::cout << "frame_of_reference:" << std::endl;
    LIST_OF_PRODUCT_CONTEXT::iterator i;
    for (i = frame_of_reference.begin(); i != frame_of_reference.end(); ++i) {
	(*i)->Print(level + 2);
    }
}

STEPEntity *
Product::GetInstance(STEPWrapper *sw, int id)
{
    return new Product(sw, id);
}

STEPEntity *
Product::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool Product::LoadONBrep(ON_Brep *brep)
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
