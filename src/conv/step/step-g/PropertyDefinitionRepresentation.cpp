/*                 PropertyDefinitionRepresentation.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2012 United States Government as represented by
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
/** @file step/PropertyDefinitionRepresentation.cpp
 *
 * Routines to convert STEP "PropertyDefinitionRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#ifdef AP203e
#include "RepresentedDefinition.h"
#else
#include "PropertyDefinition.h"
#endif
#include "ProductDefinition.h"
#include "ProductDefinitionShape.h"
#include "Representation.h"
#include "PropertyDefinitionRepresentation.h"

#define CLASSNAME "PropertyDefinitionRepresentation"
#define ENTITYNAME "Property_Definition_Representation"
string PropertyDefinitionRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) PropertyDefinitionRepresentation::Create);

PropertyDefinitionRepresentation::PropertyDefinitionRepresentation()
{
    step = NULL;
    id = 0;
    definition = NULL;
    used_representation = NULL;
}

PropertyDefinitionRepresentation::PropertyDefinitionRepresentation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    definition = NULL;
    used_representation = NULL;
}

PropertyDefinitionRepresentation::~PropertyDefinitionRepresentation()
{
#ifdef AP203e
    // not created through factory must delete here.
    if (definition)
	delete definition;
#else
    // created through factory will be deleted there.
    definition = NULL;
#endif
    used_representation = NULL;
}

string
PropertyDefinitionRepresentation::ClassName()
{
    return entityname;
}

string
PropertyDefinitionRepresentation::GetProductName()
{
    string name = "";

    if (definition) {
	ProductDefinitionShape *aPDS = dynamic_cast<ProductDefinitionShape *>(definition);
	ProductDefinition *aPD = dynamic_cast<ProductDefinition *>(definition);

	if (aPDS != NULL) {
	    name = aPDS->GetProductName();
	} else if (aPD != NULL) {
	    name = aPD->GetProductName();
	}
    }
    if (name.empty()) {
	if (used_representation) {
	    name = used_representation->GetRepresentationContextName();
	}
    }
    return name;
}

int
PropertyDefinitionRepresentation::GetProductId()
{
    int ret = 0;

    if (definition) {
	ProductDefinitionShape *aPDS = dynamic_cast<ProductDefinitionShape *>(definition);
	ProductDefinition *aPD = dynamic_cast<ProductDefinition *>(definition);

	if (aPDS != NULL) {
	    ret = aPDS->GetProductId();
	} else if (aPD != NULL) {
	    ret = aPD->GetProductId();
	}
    }
    return ret;
}

bool PropertyDefinitionRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

#ifdef AP203e
    if (definition == NULL) {
	SDAI_Select *select = step->getSelectAttribute(sse,"definition");
	if (select) { //this attribute is optional
	    RepresentedDefinition *aRD = new RepresentedDefinition();
	    definition = aRD;
	    if (!aRD->Load(step,select)) {
		std::cout << CLASSNAME << ":Error loading select RepresentedDefinition from PropertyDefinitionRepresentation." << std::endl;
		return false;
	    }
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'definition'." << std::endl;
	    return false;
	}
    }
#else
    if (definition == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "definition");
	if (entity) { //this attribute is optional
	    definition = dynamic_cast<PropertyDefinition *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'definition'." << std::endl;
	    return false;
	}
    }
#endif

    if (used_representation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "used_representation");
	if (entity) { //this attribute is optional
	    used_representation = dynamic_cast<Representation *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'used_representation'." << std::endl;
	    return false;
	}
    }

    return true;
}

void PropertyDefinitionRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level+1);
    std::cout << "definition:"<< std::endl;
    definition->Print(level+2);
    TAB(level+1);
    std::cout << "used_representation:"<< std::endl;
    used_representation->Print(level+2);
}

STEPEntity *
PropertyDefinitionRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new PropertyDefinitionRepresentation(sw, id);
}

STEPEntity *
PropertyDefinitionRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool PropertyDefinitionRepresentation::LoadONBrep(ON_Brep *brep)
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
