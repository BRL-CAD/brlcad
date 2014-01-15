/*                 ClosedShell.cpp
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
/** @file step/ClosedShell.cpp
 *
 * Routines to convert STEP "ClosedShell" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ClosedShell.h"
#include "AdvancedFace.h"

#define CLASSNAME "ClosedShell"
#define ENTITYNAME "Closed_Shell"
string ClosedShell::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ClosedShell::Create);

ClosedShell::ClosedShell()
{
    step = NULL;
    id = 0;
}

ClosedShell::ClosedShell(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ClosedShell::~ClosedShell()
{
}

bool
ClosedShell::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ConnectedFaceSet::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ConnectedFaceSet." << std::endl;
	return false;
    }
    return true;
}

void
ClosedShell::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ConnectedFaceSet::Print(level + 1);
}

void
ClosedShell::ReverseFaceSet()
{
    ConnectedFaceSet::ReverseFaceSet();
}

STEPEntity *
ClosedShell::GetInstance(STEPWrapper *sw, int id)
{
    return new ClosedShell(sw, id);
}

STEPEntity *
ClosedShell::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
ClosedShell::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !ConnectedFaceSet::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
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
