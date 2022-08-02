/* M A N I F O L D S U R F A C E S H A P E R E P R E S E N T A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file ManifoldSurfaceShapeRepresentation.cpp
 *
 * Brief description
 *
 */

/* interface header */
#include "./ManifoldSurfaceShapeRepresentation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"
#include "GeometricRepresentationContext.h"
#include "Axis2Placement3D.h"


#define CLASSNAME "ManifoldSurfaceShapeRepresentation"
#define ENTITYNAME "Manifold_Surface_Shape_Representation"


std::string ManifoldSurfaceShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ManifoldSurfaceShapeRepresentation::Create);


ManifoldSurfaceShapeRepresentation::ManifoldSurfaceShapeRepresentation()
{
    context_of_items.clear();
    items.clear();
    step = NULL;
    id = 0;
}


ManifoldSurfaceShapeRepresentation::ManifoldSurfaceShapeRepresentation(STEPWrapper *sw, int step_id)
{
    context_of_items.clear();
    items.clear();
    step = sw;
    id = step_id;
}


ManifoldSurfaceShapeRepresentation::~ManifoldSurfaceShapeRepresentation()
{
}

STEPEntity *
ManifoldSurfaceShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new ManifoldSurfaceShapeRepresentation(sw, id);
}

STEPEntity *
ManifoldSurfaceShapeRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}


Axis2Placement3D *
ManifoldSurfaceShapeRepresentation::GetAxis2Placement3d()
{
    std::list<RepresentationItem *>::iterator iter;
    Axis2Placement3D *axis = NULL;
    for (iter = items.begin(); iter != items.end(); iter++) {
	axis = dynamic_cast<Axis2Placement3D *>(*iter);
	if (axis != NULL)
	    break;
    }
    return axis;
}


bool
ManifoldSurfaceShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ShapeRepresentation::Load(step, sse)) {
	std::cerr << CLASSNAME << ":Error loading base class ::ShapeRepresentation." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}


void
ManifoldSurfaceShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ShapeRepresentation::Print(level);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
