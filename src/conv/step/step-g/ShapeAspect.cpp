/*                 ShapeAspect.cpp
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
/** @file step/ShapeAspect.cpp
 *
 * Routines to convert STEP "ShapeAspect" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShapeAspect.h"
#include "ProductDefinitionShape.h"

#define CLASSNAME "ShapeAspect"
#define ENTITYNAME "Shape_Aspect"
string ShapeAspect::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ShapeAspect::Create);

ShapeAspect::ShapeAspect()
{
    step = NULL;
    id = 0;
    name = "";
    description = "";
    of_shape = NULL;
    product_definitional = false;
}

ShapeAspect::ShapeAspect(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    name = "";
    description = "";
    of_shape = NULL;
    product_definitional = false;
}

ShapeAspect::~ShapeAspect()
{
    // created through factory will be deleted there.
    of_shape = NULL;
}

string ShapeAspect::ClassName()
{
    return entityname;
}

string ShapeAspect::Description()
{
    return description;
}

string ShapeAspect::GetName()
{
    return name;
}

ProductDefinitionShape *
ShapeAspect::GetOfShape()
{
    return of_shape;
}

bool
ShapeAspect::IsProductDefinitional()
{
    return product_definitional;
}

bool ShapeAspect::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");
    product_definitional = step->getLogicalAttribute(sse, "product_definitional");

    if (of_shape == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "of_shape");
	if (entity) { //this attribute is optional
	    of_shape = dynamic_cast<ProductDefinitionShape *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'of_shape'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ShapeAspect::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;
    TAB(level + 1);
    std::cout << "product_definitional:" << product_definitional << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;

    TAB(level + 1);
    std::cout << "of_shape:" << std::endl;
    of_shape->Print(level + 1);
}

STEPEntity *
ShapeAspect::GetInstance(STEPWrapper *sw, int id)
{
    return new ShapeAspect(sw, id);
}

STEPEntity *
ShapeAspect::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ShapeAspect::LoadONBrep(ON_Brep *brep)
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
