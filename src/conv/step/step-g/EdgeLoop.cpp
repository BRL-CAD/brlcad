/*                 EdgeLoop.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2016 United States Government as represented by
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
/** @file step/EdgeLoop.cpp
 *
 * Routines to convert STEP "EdgeLoop" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "EdgeLoop.h"
#include "OrientedEdge.h"

#define CLASSNAME "EdgeLoop"
#define ENTITYNAME "Edge_Loop"
string EdgeLoop::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)EdgeLoop::Create);

EdgeLoop::EdgeLoop()
{
    step = NULL;
    id = 0;
}

EdgeLoop::EdgeLoop(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

EdgeLoop::~EdgeLoop()
{
}

bool
EdgeLoop::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Path::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Path." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
EdgeLoop::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Path::Print(level + 1);
}

STEPEntity *
EdgeLoop::GetInstance(STEPWrapper *sw, int id)
{
    return new EdgeLoop(sw, id);
}

STEPEntity *
EdgeLoop::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

ON_BoundingBox *
EdgeLoop::GetEdgeBounds(ON_Brep *brep)
{
    return Path::GetEdgeBounds(brep);
}

bool
EdgeLoop::LoadONBrep(ON_Brep *brep)
{
    SetPathIndex(ON_loop_index);

    if (!Path::LoadONBrep(brep)) {
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
