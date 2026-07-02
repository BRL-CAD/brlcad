/*                 PolyLoop.cpp
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
/** @file step/PolyLoop.cpp
 *
 * Routines to convert STEP "PolyLoop" to BRL-CAD BREP
 * structures.
 *
 * A poly_loop is a loop whose bounding curve is defined by an ordered,
 * closed polygon of vertices (cartesian_point list).  It is the loop
 * type used by faceted_brep (planar polyhedral / mesh) geometry.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "CartesianPoint.h"

#include "PolyLoop.h"

#define CLASSNAME "PolyLoop"
#define ENTITYNAME "Poly_Loop"
string PolyLoop::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)PolyLoop::Create);

PolyLoop::PolyLoop()
{
    step = NULL;
    id = 0;
}

PolyLoop::PolyLoop(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

PolyLoop::~PolyLoop()
{
    // elements created through factory will be deleted there.
    polygon.clear();
}

bool
PolyLoop::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Loop::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Loop." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (polygon.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "polygon");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw, entity));
		if (aCP) {
		    polygon.push_back(aCP);
		} else {
		    std::cerr << CLASSNAME << ": Unhandled entity in attribute 'polygon'." << std::endl;
		    l->clear();
		    delete l;
		    sw->entity_status[id] = STEP_LOAD_ERROR;
		    return false;
		}
	    } else {
		std::cerr << CLASSNAME << ": Unhandled entity in attribute 'polygon'." << std::endl;
		l->clear();
		delete l;
		sw->entity_status[id] = STEP_LOAD_ERROR;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
PolyLoop::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "polygon:" << std::endl;
    LIST_OF_POINTS::iterator i;
    for (i = polygon.begin(); i != polygon.end(); ++i) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Loop::Print(level + 1);
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
    /* poly_loop geometry is imported as a faceted BoT mesh via FacetedBrep
     * rather than through the OpenNURBS BRep trimming path; see
     * FacetedBrep::GetBoT().  When a poly_loop is encountered directly on the
     * BRep path (rare) there is nothing to add here.
     */
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
