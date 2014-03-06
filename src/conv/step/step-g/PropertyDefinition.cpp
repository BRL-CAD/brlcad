/*                 PropertyDefinition.cpp
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
/** @file step/PropertyDefinition.cpp
 *
 * Routines to convert STEP "PropertyDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CharacterizedDefinition.h"
#include "PropertyDefinition.h"

#define CLASSNAME "PropertyDefinition"
#define ENTITYNAME "Property_Definition"
string PropertyDefinition::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) PropertyDefinition::Create);

PropertyDefinition::PropertyDefinition()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
    definition = NULL;
}

PropertyDefinition::PropertyDefinition(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
    definition = NULL;
}

PropertyDefinition::~PropertyDefinition()
{
    // not created through factory must delete here.
    if (definition)
	delete definition;
}

string PropertyDefinition::ClassName()
{
    return entityname;
}

bool PropertyDefinition::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    if (definition == NULL) {
	SDAI_Select *select = step->getSelectAttribute(sse, "definition");
	if (select) {
	    CharacterizedDefinition *aCD = new CharacterizedDefinition();

	    definition = aCD;
	    if (!aCD->Load(step, select)) {
		std::cout << CLASSNAME << ":Error loading select attribute 'definition' as CharacterizedDefinition from PropertyDefinition." << std::endl;
		return false;
	    }
	}
    }

    return true;
}

void PropertyDefinition::Print(int level)
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
    std::cout << "definition:" << std::endl;
    definition->Print(level+2);
}

STEPEntity *
PropertyDefinition::GetInstance(STEPWrapper *sw, int id)
{
    return new PropertyDefinition(sw, id);
}

STEPEntity *
PropertyDefinition::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool PropertyDefinition::LoadONBrep(ON_Brep *brep)
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
