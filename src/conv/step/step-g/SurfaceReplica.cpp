/*                 SurfaceReplica.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
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
/** @file step/SurfaceReplica.cpp
 *
 * Routines to interface to STEP "SurfaceReplica".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianTransformationOperator3D.h"

#include "SurfaceReplica.h"

#define CLASSNAME "SurfaceReplica"
#define ENTITYNAME "Surface_Replica"
string SurfaceReplica::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SurfaceReplica::Create);

SurfaceReplica::SurfaceReplica()
{
    step = NULL;
    id = 0;
    parent_surface = NULL;
    transformation = NULL;
}

SurfaceReplica::SurfaceReplica(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    parent_surface = NULL;
    transformation = NULL;
}

SurfaceReplica::~SurfaceReplica()
{
}

bool
SurfaceReplica::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!Surface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (parent_surface == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "parent_surface");
	if (entity) {
	    parent_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw, entity));
	}
	if (!entity || !parent_surface) {
	    std::cerr << CLASSNAME << ": error loading 'parent_surface' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }
    if (transformation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "transformation");
	if (entity) {
	    transformation = dynamic_cast<CartesianTransformationOperator3D *>(Factory::CreateObject(sw, entity));
	}
	if (!entity || !transformation) {
	    std::cerr << CLASSNAME << ": error loading 'transformation' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
SurfaceReplica::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "parent_surface:" << std::endl;
    parent_surface->Print(level + 1);
    TAB(level + 1);
    std::cout << "transformation:" << std::endl;
    transformation->Print(level + 1);

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Surface::Print(level + 1);
}

STEPEntity *
SurfaceReplica::GetInstance(STEPWrapper *sw, int id)
{
    return new SurfaceReplica(sw, id);
}

STEPEntity *
SurfaceReplica::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
SurfaceReplica::LoadONBrep(ON_Brep *brep)
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
