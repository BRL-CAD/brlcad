/*                 BoundedSurface.cpp
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
/** @file step/BoundedSurface.cpp
 *
 * Routines to interface to STEP "BoundedSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundedSurface.h"

#define CLASSNAME "BoundedSurface"
#define ENTITYNAME "Bounded_Surface"
string BoundedSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)BoundedSurface::Create);

BoundedSurface::BoundedSurface()
{
    step = NULL;
    id = 0;
}

BoundedSurface::BoundedSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

BoundedSurface::~BoundedSurface()
{
}

bool
BoundedSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Surface::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	return false;
    }

    return true;
}

void
BoundedSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Surface::Print(level + 1);
}

STEPEntity *
BoundedSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new BoundedSurface(sw, id);
}

STEPEntity *
BoundedSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
BoundedSurface::LoadONBrep(ON_Brep *brep)
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
