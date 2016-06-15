/*                 ShapeAspectRelationship.cpp
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
/** @file step/ShapeAspectRelationship.cpp
 *
 * Routines to convert STEP "ShapeAspectRelationship" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShapeAspectRelationship.h"
#include "ShapeAspect.h"

#define CLASSNAME "ShapeAspectRelationship"
#define ENTITYNAME "Shape_Aspect_Relationship"
string ShapeAspectRelationship::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ShapeAspectRelationship::Create);

ShapeAspectRelationship::ShapeAspectRelationship()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
    relating_shape_aspect = NULL;
    related_shape_aspect = NULL;
}

ShapeAspectRelationship::ShapeAspectRelationship(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
    relating_shape_aspect = NULL;
    related_shape_aspect = NULL;
}

ShapeAspectRelationship::~ShapeAspectRelationship()
{
    // created through factory will be deleted there.
    relating_shape_aspect = NULL;
    related_shape_aspect = NULL;
}

string ShapeAspectRelationship::ClassName()
{
    return entityname;
}

string ShapeAspectRelationship::Description()
{
    return description;
}

string ShapeAspectRelationship::GetName()
{
    return name;
}

ShapeAspect *
ShapeAspectRelationship::GetRelatingShapeAspect()
{
    return relating_shape_aspect;
}

ShapeAspect *
ShapeAspectRelationship::GetRelatedShapeAspect()
{
    return related_shape_aspect;
}

bool ShapeAspectRelationship::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

	if (relating_shape_aspect == NULL) {
		SDAI_Application_instance *entity = step->getEntityAttribute(sse,"relating_shape_aspect");
		if (entity) { //this attribute is optional
			relating_shape_aspect = dynamic_cast<ShapeAspect *>(Factory::CreateObject(sw, entity));
		} else {
			std::cout << CLASSNAME << ":Error loading attribute 'relating_shape_aspect'." << std::endl;
			return false;
		}
	}

	if (related_shape_aspect == NULL) {
		SDAI_Application_instance *entity = step->getEntityAttribute(sse,"related_shape_aspect");
		if (entity) { //this attribute is optional
			related_shape_aspect = dynamic_cast<ShapeAspect *>(Factory::CreateObject(sw, entity));
		} else {
			std::cout << CLASSNAME << ":Error loading attribute 'related_shape_aspect'." << std::endl;
			return false;
		}
	}

    return true;
}

void ShapeAspectRelationship::Print(int level)
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
    std::cout << "relating_shape_aspect:" << std::endl;
    relating_shape_aspect->Print(level + 1);

    TAB(level + 1);
    std::cout << "related_shape_aspect:" << std::endl;
    related_shape_aspect->Print(level + 1);
}

STEPEntity *
ShapeAspectRelationship::GetInstance(STEPWrapper *sw, int id)
{
    return new ShapeAspect(sw, id);
}

STEPEntity *
ShapeAspectRelationship::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ShapeAspectRelationship::LoadONBrep(ON_Brep *brep)
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
