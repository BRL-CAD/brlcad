/*                 P O L Y L O O P . C P P
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

#include "STEPWrapper.h"
#include "Factory.h"
#include "CartesianPoint.h"
#include "PolyLoop.h"

#define CLASSNAME "PolyLoop"
#define ENTITYNAME "Poly_Loop"

string PolyLoop::entityname = Factory::RegisterClass(ENTITYNAME,
    (FactoryMethod)PolyLoop::Create);

PolyLoop::PolyLoop()
{
    step = NULL;
    id = 0;
}

PolyLoop::PolyLoop(STEPWrapper *sw, int step_id) : Loop(sw, step_id)
{
}

PolyLoop::~PolyLoop()
{
    polygon.clear();
}

bool
PolyLoop::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!Loop::Load(sw, sse)) {
	sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    step = sw;
    id = sse->STEPfile_id;
    sse = step->getEntity(sse, ENTITYNAME);
    if (!sse) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    if (polygon.empty()) {
	LIST_OF_ENTITIES *entities = step->getListOfEntities(sse, "polygon");
	if (!entities) {
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
	for (LIST_OF_ENTITIES::iterator entity = entities->begin();
		entity != entities->end(); ++entity) {
	    CartesianPoint *point = *entity ? dynamic_cast<CartesianPoint *>(
		Factory::CreateObject(sw, *entity)) : NULL;
	    if (!point) {
		entities->clear();
		delete entities;
		sw->entity_status[id] = STEP_LOAD_ERROR;
		return false;
	    }
	    polygon.push_back(point);
	}
	entities->clear();
	delete entities;
    }
    if (polygon.size() < 3) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
PolyLoop::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(ID:" << STEPid() << ')'
	<< std::endl;
    for (LIST_OF_POINTS::iterator point = polygon.begin();
	    point != polygon.end(); ++point)
	(*point)->Print(level + 1);
}

STEPEntity *
PolyLoop::GetInstance(STEPWrapper *sw, int id)
{
    return new PolyLoop(sw, id);
}

STEPEntity *
PolyLoop::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
PolyLoop::LoadONBrep(ON_Brep *UNUSED(brep))
{
    /* FacetedBrep builds these loops together with their supporting planes so
     * they enter the normal validation and CDT pipeline transactionally. */
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
