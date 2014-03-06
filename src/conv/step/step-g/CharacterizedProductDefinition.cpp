/*                 CharacterizedProductDefinition.cpp
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
/** @file step/CharacterizedProductDefinition.cpp
 *
 * Routines to convert STEP "CharacterizedProductDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Axis2Placement.h"

#include "CharacterizedProductDefinition.h"
#include "ProductDefinition.h"
#include "ProductDefinitionRelationship.h"

#define CLASSNAME "CharacterizedProductDefinition"
#define ENTITYNAME "Characterized_Product_Definition"
string CharacterizedProductDefinition::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CharacterizedProductDefinition::Create);

CharacterizedProductDefinition::CharacterizedProductDefinition() {
    step = NULL;
    id = 0;
    definition = NULL;
    type = CharacterizedProductDefinition::UNKNOWN;
}

CharacterizedProductDefinition::CharacterizedProductDefinition(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    definition = NULL;
    type = CharacterizedProductDefinition::UNKNOWN;
}

CharacterizedProductDefinition::~CharacterizedProductDefinition() {
    // created through factory will be deleted there.
    definition = NULL;
}

ProductDefinition *
CharacterizedProductDefinition::GetProductDefinition() {
    return dynamic_cast<ProductDefinition *>(definition);
}

ProductDefinitionRelationship *
CharacterizedProductDefinition::GetProductDefinitionRelationship() {
    return dynamic_cast<ProductDefinitionRelationship *>(definition);
}

string
CharacterizedProductDefinition::GetProductName()
{
    string pname = "";

    if (type == CharacterizedProductDefinition::PRODUCT_DEFINITION) {
	ProductDefinition *aPD = GetProductDefinition();
	if (aPD) {
	    pname = aPD->GetProductName();
	}
    }
    return pname;
}

bool
CharacterizedProductDefinition::Load(STEPWrapper *sw,SDAI_Select *sse) {
    step=sw;

    if (definition == NULL) {
	SdaiCharacterized_product_definition *v = (SdaiCharacterized_product_definition *) sse;

	if (v->IsProduct_definition()) {
	    SdaiProduct_definition *pd = *v;
	    type = CharacterizedProductDefinition::PRODUCT_DEFINITION;
	    definition = dynamic_cast<ProductDefinition *>(Factory::CreateObject(sw, (SDAI_Application_instance *)pd));
	} else if (v->IsProduct_definition_relationship()) {
	    SdaiProduct_definition_relationship *pdr = *v;
	    type = CharacterizedProductDefinition::PRODUCT_DEFINITION_RELATIONSHIP;
	    definition = dynamic_cast<ProductDefinitionRelationship *>(Factory::CreateObject(sw, (SDAI_Application_instance *)pdr));
	} else {
	    type = CharacterizedProductDefinition::UNKNOWN;
	    definition = NULL;
	}
    }

    return true;
}

void
CharacterizedProductDefinition::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << entityname << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    //todo: expand out to print actual select type
    TAB(level+1); std::cout << "definition: ";
    if (type == CharacterizedProductDefinition::PRODUCT_DEFINITION) {
	std::cout << "PRODUCT_DEFINITION" << std::endl;
	ProductDefinition *aPD = dynamic_cast<ProductDefinition *>(definition);
	if (aPD) {
	    aPD->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "PRODUCT_DEFINITION == NULL" << std::endl;
	}
    } else if (type == CharacterizedProductDefinition::PRODUCT_DEFINITION_RELATIONSHIP) {
	std::cout << "PRODUCT_DEFINITION_RELATIONSHIP" << std::endl;
	ProductDefinitionRelationship *aPDR = dynamic_cast<ProductDefinitionRelationship *>(definition);
	if (aPDR) {
	    aPDR->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "PRODUCT_DEFINITION_RELATIONSHIP == NULL" << std::endl;
	}
    } else {
	std::cout << "UNKNOWN" << std::endl;
    }
}

STEPEntity *
CharacterizedProductDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new CharacterizedProductDefinition(sw, id);
}

STEPEntity *
CharacterizedProductDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
CharacterizedProductDefinition::LoadONBrep(ON_Brep *brep)
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
