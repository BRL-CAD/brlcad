/*                 Face.cpp
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
/** @file step/Face.cpp
 *
 * Routines to convert STEP "Face" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Face.h"
#include "QuasiUniformSurface.h"
#include "FaceOuterBound.h"

#define CLASSNAME "Face"
#define ENTITYNAME "Face"
string Face::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Face::Create);

Face::Face()
{
    step = NULL;
    id = 0;
    reverse = false;
}

Face::Face(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    reverse = false;
}

Face::~Face()
{
    // elements created through factory will be deleted there.
    bounds.clear();
}

bool
Face::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!TopologicalRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (bounds.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "bounds");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		FaceBound *aFB = dynamic_cast<FaceBound *>(Factory::CreateObject(sw, entity));
		if (aFB) {
		    bounds.push_back(aFB);
		} else {
		    l->clear();
		    sw->entity_status[id] = STEP_LOAD_ERROR;
		    delete l;
		    return false;
		}
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'bounds'." << std::endl;
		l->clear();
		sw->entity_status[id] = STEP_LOAD_ERROR;
		delete l;
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
Face::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "bounds:" << std::endl;
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    TopologicalRepresentationItem::Print(level + 1);
}

void
Face::ReverseFace()
{
    reverse = true;
}

STEPEntity *
Face::GetInstance(STEPWrapper *sw, int id)
{
    return new Face(sw, id);
}

STEPEntity *
Face::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
