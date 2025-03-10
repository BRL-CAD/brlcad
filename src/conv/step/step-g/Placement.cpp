/*                 Placement.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2025 United States Government as represented by
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
/** @file step/Placement.cpp
 *
 * Routines to convert STEP "Placement" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"

#include "Placement.h"

#define CLASSNAME "Placement"
#define ENTITYNAME "Placement"
string Placement::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Placement::Create);

Placement::Placement()
{
    step = NULL;
    id = 0;
    location = NULL;
}

Placement::Placement(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    location = NULL;
}

Placement::~Placement()
{
    location = NULL;
}

const double *
Placement::GetOrigin()
{
    return NULL;
}

const double *
Placement::GetNormal()
{
    return NULL;
}

const double *
Placement::GetXAxis()
{
    return NULL;
}

const double *
Placement::GetYAxis()
{
    return NULL;
}

bool
Placement::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (location == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "location");
	if (entity) {
	    location = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'location'." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
Placement::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << std::endl;
    location->Print(level + 1);
}

STEPEntity *
Placement::GetInstance(STEPWrapper *sw, int id)
{
    return new Placement(sw, id);
}

STEPEntity *
Placement::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
