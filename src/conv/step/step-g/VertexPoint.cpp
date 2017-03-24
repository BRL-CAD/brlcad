/*                 VertexPoint.cpp
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
/** @file step/VertexPoint.cpp
 *
 * Routines to convert STEP "VertexPoint" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "VertexPoint.h"
#include "CartesianPoint.h"

#define CLASSNAME "VertexPoint"
#define ENTITYNAME "Vertex_Point"
string VertexPoint::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)VertexPoint::Create);

VertexPoint::VertexPoint()
{
    step = NULL;
    id = 0;
    vertex_geometry = NULL;
}

VertexPoint::VertexPoint(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    vertex_geometry = NULL;
}

VertexPoint::~VertexPoint()
{
    vertex_geometry = NULL;
}

bool
VertexPoint::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!Vertex::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Vertex." << std::endl;
	goto step_error;
    }
    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	goto step_error;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (vertex_geometry == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "vertex_geometry");
	if (entity) {
	    vertex_geometry = dynamic_cast<Point *>(Factory::CreateObject(sw, entity));
	}
	if (!entity || !vertex_geometry) {
	    std::cout << CLASSNAME << ":Error loading attribute 'vertex_geometry'." << std::endl;
	    goto step_error;
	}
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

void
VertexPoint::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "vertex_geometry:" << std::endl;
    if (vertex_geometry) {
	vertex_geometry->Print(level + 1);
    } else {
	TAB(level);
	std::cout << "vertex_geometry:NULL" << std::endl;
    }
}

STEPEntity *
VertexPoint::GetInstance(STEPWrapper *sw, int id)
{
    return new VertexPoint(sw, id);
}

STEPEntity *
VertexPoint::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
VertexPoint::LoadONBrep(ON_Brep *brep)
{
    if (!vertex_geometry->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    ON_id = vertex_geometry->GetONId();
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
