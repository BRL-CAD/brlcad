/*                 ShapeDefinitionRepresentation.cpp
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
/** @file step/ShapeDefinitionRepresentation.cpp
 *
 * Routines to convert STEP "ShapeDefinitionRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShapeDefinitionRepresentation.h"
#include "ShapeRepresentation.h"
#include "AdvancedBrepShapeRepresentation.h"

#define CLASSNAME "ShapeDefinitionRepresentation"
#define ENTITYNAME "Shape_Definition_Representation"
string ShapeDefinitionRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ShapeDefinitionRepresentation::Create);

ShapeDefinitionRepresentation::ShapeDefinitionRepresentation()
{
    step = NULL;
    id = 0;
}

ShapeDefinitionRepresentation::ShapeDefinitionRepresentation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ShapeDefinitionRepresentation::~ShapeDefinitionRepresentation()
{
}

string ShapeDefinitionRepresentation::ClassName()
{
    return entityname;
}

AdvancedBrepShapeRepresentation *
ShapeDefinitionRepresentation::GetAdvancedBrepShapeRepresentation()
{
    return dynamic_cast<AdvancedBrepShapeRepresentation *>(used_representation);
}

ShapeRepresentation *
ShapeDefinitionRepresentation::GetShapeRepresentation()
{
    return dynamic_cast<ShapeRepresentation *>(used_representation);
}

bool ShapeDefinitionRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!PropertyDefinitionRepresentation::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::PropertyDefinitionRepresentation." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void ShapeDefinitionRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    PropertyDefinitionRepresentation::Print(level + 2);
}

STEPEntity *
ShapeDefinitionRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new ShapeDefinitionRepresentation(sw, id);
}

STEPEntity *
ShapeDefinitionRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse) {
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ShapeDefinitionRepresentation::LoadONBrep(ON_Brep *brep)
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
