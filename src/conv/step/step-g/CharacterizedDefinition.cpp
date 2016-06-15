/*                 CharacterizedDefinition.cpp
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
/** @file step/CharacterizedDefinition.cpp
 *
 * Routines to convert STEP "CharacterizedDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Axis2Placement.h"

#include "CharacterizedDefinition.h"
#include "CharacterizedProductDefinition.h"
#include "ShapeDefinition.h"

#define CLASSNAME "CharacterizedDefinition"
#define ENTITYNAME "Characterized_Definition"
string CharacterizedDefinition::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CharacterizedDefinition::Create);

CharacterizedDefinition::CharacterizedDefinition() {
    step = NULL;
    id = 0;
    definition = NULL;
    type = CharacterizedDefinition::UNKNOWN;
}

CharacterizedDefinition::CharacterizedDefinition(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    definition = NULL;
    type = CharacterizedDefinition::UNKNOWN;
}

CharacterizedDefinition::~CharacterizedDefinition() {
#ifdef AP203e
    if (type == CharacterizedDefinition::CHARACTERIZED_OBJECT)
    {
	definition = NULL;
    }
#endif

    if (definition)
	delete definition;
}

CharacterizedProductDefinition *
CharacterizedDefinition::GetCharacterizedProductDefinition() {
    return dynamic_cast<CharacterizedProductDefinition *>(definition);
}

ShapeDefinition *
CharacterizedDefinition::GetShapeDefinition() {
    return dynamic_cast<ShapeDefinition *>(definition);
}

string
CharacterizedDefinition::GetProductName()
{
    string pname = "";
    if (type == CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION) {
	CharacterizedProductDefinition *aCPD = GetCharacterizedProductDefinition();
	if (aCPD) {
	    pname = aCPD->GetProductName();
	}
    }
    return pname;
}

int
CharacterizedDefinition::GetProductId()
{
    int ret = 0;
    if (type == CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION) {
	CharacterizedProductDefinition *aCPD = GetCharacterizedProductDefinition();
	if (aCPD) {
	    ret = aCPD->GetProductId();
	}
    }
    return ret;
}

ProductDefinition *
CharacterizedDefinition::GetRelatingProductDefinition()
{
    ProductDefinition *ret = NULL;

    if (type == CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION) {
	CharacterizedProductDefinition *aCPD = GetCharacterizedProductDefinition();
	if (aCPD) {
	    ret = aCPD->GetRelatingProductDefinition();
	}
    }

    return ret;
}

ProductDefinition *
CharacterizedDefinition::GetRelatedProductDefinition()
{
    ProductDefinition *ret = NULL;

    if (type == CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION) {
	CharacterizedProductDefinition *aCPD = GetCharacterizedProductDefinition();
	if (aCPD) {
	    ret = aCPD->GetRelatedProductDefinition();
	}
    }

    return ret;
}

bool
CharacterizedDefinition::Load(STEPWrapper *sw,SDAI_Select *sse) {
    step=sw;

    if (definition == NULL) {
	SdaiCharacterized_definition *v = (SdaiCharacterized_definition *) sse;

	if (v->IsCharacterized_product_definition()) {
	    SdaiCharacterized_product_definition *cpd_select = *v;
	    CharacterizedProductDefinition *aCPD = new CharacterizedProductDefinition();

	    type = CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION;
	    definition = aCPD;
	    if (!aCPD->Load(step, cpd_select)) {
		std::cout << CLASSNAME << ":Error loading select attribute 'definition' as CharacterizedProductDefinition from CharacterizedDefinition." << std::endl;
		return false;
	    }
	} else if (v->IsShape_definition()) {
	    SdaiShape_definition *sd_select = *v;
	    ShapeDefinition *aSD = new ShapeDefinition();

	    type = CharacterizedDefinition::SHAPE_DEFINITION;
	    definition = aSD;
	    if (!aSD->Load(step, sd_select)) {
		std::cout << CLASSNAME << ":Error loading select attribute 'definition' as CharacterizedProductDefinition from CharacterizedDefinition." << std::endl;
		return false;
	    }
#ifdef AP203e
	} else if (v->IsCharacterized_object()) {
	    SdaiCharacterized_object *co = *v;
	    type = CharacterizedDefinition::CHARACTERIZED_OBJECT;
	    definition = dynamic_cast<CharacterizedObject *>(Factory::CreateObject(sw, (SDAI_Application_instance *)pdr));
#endif
	} else {
	    type = CharacterizedDefinition::UNKNOWN;
	    definition = NULL;
	}
    }

    return true;
}

void
CharacterizedDefinition::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << entityname << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    //todo: expand out to print actual select type
    TAB(level+1); std::cout << "definition: ";

    if (type == CharacterizedDefinition::CHARACTERIZED_PRODUCT_DEFINITION) {
	std::cout << "CHARACTERIZED_PRODUCT_DEFINITION" << std::endl;
	CharacterizedProductDefinition *aCPD = dynamic_cast<CharacterizedProductDefinition *>(definition);
	if (aCPD) {
	    aCPD->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "CHARACTERIZED_PRODUCT_DEFINITION == NULL" << std::endl;
	}
    } else if (type == CharacterizedDefinition::SHAPE_DEFINITION) {
	std::cout << "SHAPE_DEFINITION" << std::endl;
	ShapeDefinition *aSD = dynamic_cast<ShapeDefinition *>(definition);
	if (aSD) {
	    aSD->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "SHAPE_DEFINITION == NULL" << std::endl;
	}
#ifdef AP203e
    } else if (type == CharacterizedDefinition::CHARACTERIZED_OBJECT) {
	std::cout << "CHARACTERIZED_OBJECT" << std::endl;
	CharacterizedObject *co = dynamic_cast<CharacterizedObject *>(definition);
	if (co) {
	    co->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "CHARACTERIZED_OBJECT == NULL" << std::endl;
	}
#endif
    } else {
	std::cout << "UNKNOWN" << std::endl;
    }
}

STEPEntity *
CharacterizedDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new CharacterizedDefinition(sw, id);
}

STEPEntity *
CharacterizedDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
CharacterizedDefinition::LoadONBrep(ON_Brep *brep)
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
