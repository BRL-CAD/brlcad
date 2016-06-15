/*                 ShapeDefinition.cpp
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
/** @file step/ShapeDefinition.cpp
 *
 * Routines to convert STEP "ShapeDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShapeDefinition.h"
#include "ProductDefinitionShape.h"
#include "ShapeAspect.h"
#include "ShapeAspectRelationship.h"

#define CLASSNAME "ShapeDefinition"
#define ENTITYNAME "Shape_Definition"
string ShapeDefinition::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)ShapeDefinition::Create);

ShapeDefinition::ShapeDefinition() {
    step = NULL;
    id = 0;
    definition = NULL;
    type = ShapeDefinition::UNKNOWN;
}

ShapeDefinition::ShapeDefinition(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    definition = NULL;
    type = ShapeDefinition::UNKNOWN;
}

ShapeDefinition::~ShapeDefinition() {
    // created through factory will be deleted there.
    definition = NULL;
}
/*
ProductDefinition *
ShapeDefinition::GetProductDefinition() {
    return dynamic_cast<ProductDefinition *>(definition);
}

ProductDefinitionRelationship *
ShapeDefinition::GetProductDefinitionRelationship() {
    return dynamic_cast<ProductDefinitionRelationship *>(definition);
}
*/
bool
ShapeDefinition::Load(STEPWrapper *sw,SDAI_Select *sse) {
    step=sw;

    if (definition == NULL) {
	SdaiShape_definition *v = (SdaiShape_definition *) sse;

	if (v->IsProduct_definition_shape()) {
	    SdaiProduct_definition_shape *pds = *v;
	    type = ShapeDefinition::PRODUCT_DEFINITION_SHAPE;
	    definition = dynamic_cast<ProductDefinitionShape *>(Factory::CreateObject(sw, (SDAI_Application_instance *)pds));
	} else if (v->IsShape_aspect()) {
	    SdaiShape_aspect *sa = *v;
	    type = ShapeDefinition::SHAPE_ASPECT;
	    definition = dynamic_cast<ShapeAspect *>(Factory::CreateObject(sw, (SDAI_Application_instance *)sa));
	} else if (v->IsShape_aspect_relationship()) {
	    SdaiShape_aspect_relationship *sar = *v;
	    type = ShapeDefinition::SHAPE_ASPECT_RELATIONSHIP;
	    definition = dynamic_cast<ShapeAspectRelationship *>(Factory::CreateObject(sw, (SDAI_Application_instance *)sar));
	} else {
	    type = ShapeDefinition::UNKNOWN;
	    definition = NULL;
	}
    }

    return true;
}

void
ShapeDefinition::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << entityname << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    //todo: expand out to print actual select type
    TAB(level+1); std::cout << "definition: ";
    if (type == ShapeDefinition::PRODUCT_DEFINITION_SHAPE) {
	std::cout << "PRODUCT_DEFINITION_SHAPE" << std::endl;
	ProductDefinitionShape *aPDS = dynamic_cast<ProductDefinitionShape *>(definition);
	if (aPDS) {
	    aPDS->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "PRODUCT_DEFINITION_SHAPE == NULL" << std::endl;
	}
    } else if (type == ShapeDefinition::SHAPE_ASPECT) {
	std::cout << "SHAPE_ASPECT" << std::endl;
	ShapeAspect *aSA = dynamic_cast<ShapeAspect *>(definition);
	if (aSA) {
	    aSA->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "SHAPE_ASPECT == NULL" << std::endl;
	}
    } else if (type == ShapeDefinition::SHAPE_ASPECT_RELATIONSHIP) {
	std::cout << "SHAPE_ASPECT_RELATIONSHIP" << std::endl;
	ShapeAspectRelationship *aSAR = dynamic_cast<ShapeAspectRelationship *>(definition);
	if (aSAR) {
	    aSAR->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "SHAPE_ASPECT_RELATIONSHIP == NULL" << std::endl;
	}
    } else {
	std::cout << "UNKNOWN" << std::endl;
    }

}

STEPEntity *
ShapeDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new ShapeDefinition(sw, id);
}

STEPEntity *
ShapeDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
ShapeDefinition::LoadONBrep(ON_Brep *brep)
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
