/*                 ApplicationContextElement.cpp
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
/** @file step/ApplicationContextElement.cpp
 *
 * Routines to convert STEP "ApplicationContextElement" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ApplicationContext.h"
#include "ApplicationContextElement.h"

#define CLASSNAME "ApplicationContextElement"
#define ENTITYNAME "Application_Context_Element"
string ApplicationContextElement::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ApplicationContextElement::Create);

ApplicationContextElement::ApplicationContextElement()
{
    step = NULL;
    id = 0;
    name = "";
    frame_of_reference = NULL;
}

ApplicationContextElement::ApplicationContextElement(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    frame_of_reference = NULL;
}

ApplicationContextElement::~ApplicationContextElement()
{
}

string ApplicationContextElement::ClassName()
{
    return entityname;
}

string ApplicationContextElement::Name()
{
    return name;
}

bool ApplicationContextElement::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    if (frame_of_reference == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "frame_of_reference");
	if (entity) { //this attribute is optional
	    frame_of_reference = dynamic_cast<ApplicationContext *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'Application_Context_Element'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ApplicationContextElement::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;

    TAB(level + 1);
    std::cout << "frame_of_reference:" << std::endl;
    frame_of_reference->Print(level + 2);
}

STEPEntity *
ApplicationContextElement::GetInstance(STEPWrapper *sw, int id)
{
    return new ApplicationContextElement(sw, id);
}

STEPEntity *
ApplicationContextElement::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ApplicationContextElement::LoadONBrep(ON_Brep *brep)
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
