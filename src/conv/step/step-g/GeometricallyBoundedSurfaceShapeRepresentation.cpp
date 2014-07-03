/*                 GeometricallyBoundedSurfaceShapeRepresentation.cpp
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
/** @file step/GeometricallyBoundedSurfaceShapeRepresentation.cpp
 *
 * Routines to convert STEP "GeometricallyBoundedSurfaceShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

/* interface header */
#include "GeometricallyBoundedSurfaceShapeRepresentation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"
#include "ManifoldSolidBrep.h"
#include "GeometricRepresentationContext.h"


#define BUFSIZE 255 // used for buffer size that stringifies step ID
#define CLASSNAME "GeometricallyBoundedSurfaceShapeRepresentation"
#define ENTITYNAME "Geometrically_Bounded_Surface_Shape_Representation"

std::string GeometricallyBoundedSurfaceShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)GeometricallyBoundedSurfaceShapeRepresentation::Create);


GeometricallyBoundedSurfaceShapeRepresentation::GeometricallyBoundedSurfaceShapeRepresentation()
{
    step = NULL;
    id = 0;
}


GeometricallyBoundedSurfaceShapeRepresentation::GeometricallyBoundedSurfaceShapeRepresentation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}


GeometricallyBoundedSurfaceShapeRepresentation::~GeometricallyBoundedSurfaceShapeRepresentation()
{
}


bool
GeometricallyBoundedSurfaceShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ShapeRepresentation::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading baseclass Representation." << std::endl;
	return false;
    }

    return true;
}


void
GeometricallyBoundedSurfaceShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    ShapeRepresentation::Print(level);
}

STEPEntity *
GeometricallyBoundedSurfaceShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricallyBoundedSurfaceShapeRepresentation(sw, id);
}

STEPEntity *
GeometricallyBoundedSurfaceShapeRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
