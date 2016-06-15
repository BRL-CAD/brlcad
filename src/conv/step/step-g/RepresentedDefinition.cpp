/*                 RepresentedDefinition.cpp
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
/** @file step/RepresentedDefinition.cpp
 *
 * Routines to convert STEP "RepresentedDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RepresentedDefinition.h"

#define CLASSNAME "RepresentedDefinition"
#define ENTITYNAME "Represented_Definition"
string RepresentedDefinition::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)RepresentedDefinition::Create);

const char *represented_definition_type_names[] = {
    "GENERAL_PROPERTY",
    "PROPERTY_DEFINITION",
    "PROPERTY_DEFINITION_RELATIONSHIP",
    "SHAPE_ASPECT",
    "SHAPE_ASPECT_RELATIONSHIP",
    NULL
};

RepresentedDefinition::RepresentedDefinition() {
    step = NULL;
    id = 0;
    entity = NULL;
    type = RepresentedDefinition::UNKNOWN;
}

RepresentedDefinition::RepresentedDefinition(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    entity = NULL;
    type = RepresentedDefinition::UNKNOWN;
}

RepresentedDefinition::~RepresentedDefinition() {
}

bool
RepresentedDefinition::Load(STEPWrapper *sw,SDAI_Select *sse) {
    step=sw;

    //TODO: Need to complete for AP203e2
    if (entity == NULL) {
	SDAI_Select *v = (SDAI_Select *)sse;
	if (v) {
	    std::cerr << CLASSNAME << ": Need to finish this up for AP203e2" << std::endl;
	}
/*
	if ( v->IsAxis2_placement_2d()) {
	    SdaiAxis2_placement_2d *a2 = *v;
	    type = RepresentedDefinition::AXIS2_PLACEMENT_2D;
	    value = dynamic_cast<Placement *>(Factory::CreateObject(sw,(SDAI_Application_instance *)a2));
	} else if (v->IsAxis2_placement_3d()) {
	    SdaiAxis2_placement_3d *a3 = *v;
	    type = RepresentedDefinition::AXIS2_PLACEMENT_3D;
	    value = dynamic_cast<Placement *>(Factory::CreateObject(sw,(SDAI_Application_instance *)a3));
	} else {
	    type = RepresentedDefinition::UNKNOWN;
	}
	*/
    }


    return true;
}

void
RepresentedDefinition::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    if (type == GENERAL_PROPERTY) {
	TAB(level+1); std::cout << "Type:" << represented_definition_type_names[type] << " Value:" << std::endl;
	//entity->Print(level+1);
    } else if (type == PROPERTY_DEFINITION) {
	TAB(level+1); std::cout << "Type:" << represented_definition_type_names[type] << " Value:" << std::endl;
	//entity->Print(level+1);
    } else if (type == PROPERTY_DEFINITION_RELATIONSHIP) {
	TAB(level+1); std::cout << "Type:" << represented_definition_type_names[type] << " Value:" << std::endl;
	//entity->Print(level+1);
    } else if (type == SHAPE_ASPECT) {
	TAB(level+1); std::cout << "Type:" << represented_definition_type_names[type] << " Value:" << std::endl;
	//entity->Print(level+1);
    } else if (type == SHAPE_ASPECT_RELATIONSHIP) {
	TAB(level+1); std::cout << "Type:" << represented_definition_type_names[type] << " Value:" << std::endl;
	//entity->Print(level+1);
    } else {
	TAB(level+1); std::cout << "Type:" << "UNKNOWN" << " Value:" << std::endl;
    }
}

STEPEntity *
RepresentedDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new RepresentedDefinition(sw, id);
}

STEPEntity *
RepresentedDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
