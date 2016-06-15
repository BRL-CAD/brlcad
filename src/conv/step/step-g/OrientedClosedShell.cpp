/*                 OrientedClosedShell.cpp
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
/** @file OrientedClosedShell.cpp
 *
 * Routines to convert STEP "OrientedClosedShell" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "OrientedClosedShell.h"

#define CLASSNAME "OrientedClosedShell"
#define ENTITYNAME "Oriented_Closed_Shell"
string OrientedClosedShell::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) OrientedClosedShell::Create);

OrientedClosedShell::OrientedClosedShell()
{
    step = NULL;
    id = 0;
    closed_shell_element = NULL;
    orientation = BFalse;
}

OrientedClosedShell::OrientedClosedShell(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    closed_shell_element = NULL;
    orientation = BFalse;
}

OrientedClosedShell::~OrientedClosedShell()
{
}

bool OrientedClosedShell::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ClosedShell::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ConnectedFaceSet." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    orientation = step->getBooleanAttribute(sse, "orientation");

    if (closed_shell_element == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "closed_shell_element");
	if (entity) {
	    closed_shell_element = dynamic_cast<ClosedShell *>(Factory::CreateObject(sw, entity));
	    if (closed_shell_element == NULL) {
		std::cerr << CLASSNAME << ": Error loading entity attribute 'closed_shell_element'." << std::endl;
		return false;
	    }
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'closed_shell_element'." << std::endl;
	    return false;
	}
    }

    if (orientation != 1) {
	closed_shell_element->ReverseFaceSet();
    }
    return true;
}

void OrientedClosedShell::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "closed_shell_element:" << std::endl;
    closed_shell_element->Print(level + 1);
    TAB(level + 1);
    std::cout << "orientation:" << step->getBooleanString(orientation) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ConnectedFaceSet::Print(level + 1);
}

STEPEntity *
OrientedClosedShell::GetInstance(STEPWrapper *sw, int id)
{
    return new OrientedClosedShell(sw, id);
}

STEPEntity *
OrientedClosedShell::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool OrientedClosedShell::LoadONBrep(ON_Brep *brep)
{
    if (ON_id >= 0) {
	return true;    //already loaded
    }

    if (!closed_shell_element->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_id = closed_shell_element->GetONId();

    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
