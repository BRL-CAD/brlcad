/*                 ShapeRepresentationRelationship.cpp
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
/** @file step/ShapeRepresentationRelationship.cpp
 *
 * Routines to convert STEP "ShapeRepresentationRelationship" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShapeRepresentationRelationship.h"

#define CLASSNAME "ShapeRepresentationRelationship"
#define ENTITYNAME "Shape_Representation_Relationship"
string ShapeRepresentationRelationship::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ShapeRepresentationRelationship::Create);

ShapeRepresentationRelationship::ShapeRepresentationRelationship()
{
    step = NULL;
    id = 0;
}

ShapeRepresentationRelationship::ShapeRepresentationRelationship(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ShapeRepresentationRelationship::~ShapeRepresentationRelationship()
{
}

string ShapeRepresentationRelationship::ClassName()
{
    return entityname;
}

bool ShapeRepresentationRelationship::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!RepresentationRelationship::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationRelationship." << std::endl;
	return false;
    }

    return true;
}

void ShapeRepresentationRelationship::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RepresentationRelationship::Print(level + 2);
}

STEPEntity *
ShapeRepresentationRelationship::GetInstance(STEPWrapper *sw, int id)
{
    return new ShapeRepresentationRelationship(sw, id);
}

STEPEntity *
ShapeRepresentationRelationship::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ShapeRepresentationRelationship::LoadONBrep(ON_Brep *brep)
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
