/*                 GeometricRepresentationItem.cpp
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
/** @file step/GeometricRepresentationItem.cpp
 *
 * Routines to convert STEP "GeometricRepresentationItem" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "GeometricRepresentationItem.h"

#define CLASSNAME "GeometricRepresentationItem"
#define ENTITYNAME "Geometric_Representation_Item"
string GeometricRepresentationItem::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)GeometricRepresentationItem::Create);

GeometricRepresentationItem::GeometricRepresentationItem()
{
    step = NULL;
    id = 0;
}

GeometricRepresentationItem::GeometricRepresentationItem(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

GeometricRepresentationItem::~GeometricRepresentationItem()
{
}

bool
GeometricRepresentationItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RepresentationItem::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationItem." << std::endl;
	return false;
    }

    return true;
}

void
GeometricRepresentationItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RepresentationItem::Print(level + 1);
}

STEPEntity *
GeometricRepresentationItem::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricRepresentationItem(sw, id);
}

STEPEntity *
GeometricRepresentationItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
GeometricRepresentationItem::LoadONBrep(ON_Brep *brep)
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
