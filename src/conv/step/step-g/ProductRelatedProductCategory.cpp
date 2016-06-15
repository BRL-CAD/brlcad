/*                 ProductRelatedProductCategory.cpp
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
/** @file step/ProductRelatedProductCategory.cpp
 *
 * Routines to convert STEP "ProductRelatedProductCategory" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Product.h"
#include "ProductRelatedProductCategory.h"

#define CLASSNAME "ProductRelatedProductCategory"
#define ENTITYNAME "Product_Related_Product_Category"
string ProductRelatedProductCategory::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ProductRelatedProductCategory::Create);

ProductRelatedProductCategory::ProductRelatedProductCategory()
{
    step = NULL;
    id = 0;
}

ProductRelatedProductCategory::ProductRelatedProductCategory(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ProductRelatedProductCategory::~ProductRelatedProductCategory()
{
    // elements created through factory will be deleted there.
    products.clear();
}

string ProductRelatedProductCategory::ClassName()
{
    return entityname;
}

bool ProductRelatedProductCategory::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ProductCategory::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ProductCategory." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (products.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "products");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		Product *aProd = dynamic_cast<Product *>(Factory::CreateObject(sw, entity));

		products.push_back(aProd);
	    } else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'products'." << std::endl;
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

void ProductRelatedProductCategory::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;
    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ProductCategory::Print(level);

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "products:" << std::endl;
    LIST_OF_PRODUCTS::iterator i;
    for (i = products.begin(); i != products.end(); ++i) {
	(*i)->Print(level + 2);
    }
}

STEPEntity *
ProductRelatedProductCategory::GetInstance(STEPWrapper *sw, int id)
{
    return new ProductRelatedProductCategory(sw, id);
}

STEPEntity *
ProductRelatedProductCategory::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ProductRelatedProductCategory::LoadONBrep(ON_Brep *brep)
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
