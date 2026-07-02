/*                 RepresentationMap.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/RepresentationMap.cpp
 *
 * Routines to convert STEP "RepresentationMap" to BRL-CAD structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RepresentationMap.h"
#include "Representation.h"
#include "Axis2Placement3D.h"

#define CLASSNAME "RepresentationMap"
#define ENTITYNAME "Representation_Map"
string RepresentationMap::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RepresentationMap::Create);

RepresentationMap::RepresentationMap()
{
    step = NULL;
    id = 0;
    mapping_origin = NULL;
    mapped_representation = NULL;
}

RepresentationMap::RepresentationMap(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    mapping_origin = NULL;
    mapped_representation = NULL;
}

RepresentationMap::~RepresentationMap()
{
    // created through factory will be deleted there.
    mapping_origin = NULL;
    mapped_representation = NULL;
}

bool
RepresentationMap::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (mapping_origin == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapping_origin");
	if (entity) {
	    mapping_origin = dynamic_cast<Axis2Placement3D *>(Factory::CreateObject(sw, entity));
	}
	// mapping_origin is only usable for placement when it is an
	// axis2_placement_3d; other representation_item kinds are tolerated
	// (mapping_origin stays NULL) and treated as identity by the caller.
    }

    if (mapped_representation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapped_representation");
	if (entity) {
	    mapped_representation = dynamic_cast<Representation *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'mapped_representation'." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
RepresentationMap::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    if (mapping_origin) {
	TAB(level + 1);
	std::cout << "mapping_origin:" << std::endl;
	mapping_origin->Print(level + 2);
    }
    if (mapped_representation) {
	TAB(level + 1);
	std::cout << "mapped_representation:" << std::endl;
	mapped_representation->Print(level + 2);
    }
}

STEPEntity *
RepresentationMap::GetInstance(STEPWrapper *sw, int id)
{
    return new RepresentationMap(sw, id);
}

STEPEntity *
RepresentationMap::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
RepresentationMap::LoadONBrep(ON_Brep *UNUSED(brep))
{
    /* nothing to draw directly; consumed by MappedItem during hierarchy build */
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
