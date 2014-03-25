/*                 RepresentationRelationshipWithTransformation.cpp
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
/** @file step/RepresentationRelationshipWithTransformation.cpp
 *
 * Routines to convert STEP "RepresentationRelationshipWithTransformation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Axis2Placement3D.h"
#include "Transformation.h"
#include "RepresentationRelationshipWithTransformation.h"
#include "ItemDefinedTransformation.h"

#define CLASSNAME "RepresentationRelationshipWithTransformation"
#define ENTITYNAME "Representation_Relationship_With_Transformation"
string RepresentationRelationshipWithTransformation::entityname = Factory::RegisterClass(ENTITYNAME,
	(FactoryMethod) RepresentationRelationshipWithTransformation::Create);

RepresentationRelationshipWithTransformation::RepresentationRelationshipWithTransformation()
{
    step = NULL;
    id = 0;
    transformation_operator = NULL;
}

RepresentationRelationshipWithTransformation::RepresentationRelationshipWithTransformation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    transformation_operator = NULL;
}

RepresentationRelationshipWithTransformation::~RepresentationRelationshipWithTransformation()
{
    // created through factory will be deleted there.
    transformation_operator = NULL;
}

string RepresentationRelationshipWithTransformation::ClassName()
{
    return entityname;
}

Axis2Placement3D *
RepresentationRelationshipWithTransformation::GetTransformItem_1()
{
    Axis2Placement3D *axis = NULL;
    if (transformation_operator != NULL) {
	ItemDefinedTransformation *idt = dynamic_cast<ItemDefinedTransformation*>(transformation_operator);

	if (idt) {
	    axis = idt->GetTransformItem_1();
	}
    }
    return axis;
}

Axis2Placement3D *
RepresentationRelationshipWithTransformation::GetTransformItem_2()
{
    Axis2Placement3D *axis = NULL;
    if (transformation_operator != NULL) {
	ItemDefinedTransformation *idt = dynamic_cast<ItemDefinedTransformation*>(transformation_operator);

	if (idt) {
	    axis = idt->GetTransformItem_2();
	}
    }
    return axis;
}

bool RepresentationRelationshipWithTransformation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!RepresentationRelationship::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationRelationship." << std::endl;
	return false;
    }
    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (transformation_operator == NULL) {
	// select-select
	SDAI_Select *select = step->getSelectAttribute(sse, "transformation_operator");
	if (select) {
	    SdaiTransformation *t = (SdaiTransformation *) select;
	    if (t->IsItem_defined_transformation()) {
		SdaiItem_defined_transformation *idt = *t;
		transformation_operator = dynamic_cast<Transformation *>(Factory::CreateObject(sw, (SDAI_Application_instance *) idt));
	    } else if (t->IsFunctionally_defined_transformation()) {
		SdaiFunctionally_defined_transformation *fdt = *t;
		transformation_operator = dynamic_cast<Transformation *>(Factory::CreateObject(sw, (SDAI_Application_instance *) fdt));
	    } else {
		std::cerr << CLASSNAME << ": Unknown 'Transformation' type from select." << std::endl;
		return false;
	    }
	} else {
	    return false;
	}
    }

    return true;
}

void RepresentationRelationshipWithTransformation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    transformation_operator->Print(level + 1);
    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RepresentationRelationship::Print(level + 2);
}

STEPEntity *
RepresentationRelationshipWithTransformation::GetInstance(STEPWrapper *sw, int id)
{
    return new RepresentationRelationshipWithTransformation(sw, id);
}

STEPEntity *
RepresentationRelationshipWithTransformation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool RepresentationRelationshipWithTransformation::LoadONBrep(ON_Brep *brep)
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
