/*                 ConnectedFaceSet.cpp
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
/** @file step/ConnectedFaceSet.cpp
 *
 * Routines to convert STEP "ConnectedFaceSet" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ConnectedFaceSet.h"
#include "AdvancedFace.h"

#define CLASSNAME "ConnectedFaceSet"
#define ENTITYNAME "Connected_Face_Set"
string ConnectedFaceSet::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ConnectedFaceSet::Create);

ConnectedFaceSet::ConnectedFaceSet()
{
    step = NULL;
    id = 0;
}

ConnectedFaceSet::ConnectedFaceSet(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ConnectedFaceSet::~ConnectedFaceSet()
{
    // elements created through factory will be deleted there.
    cfs_faces.clear();
}

bool
ConnectedFaceSet::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!TopologicalRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (cfs_faces.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "cfs_faces");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		Face *aAF = dynamic_cast<Face *>(Factory::CreateObject(sw, entity)); //CreateSurfaceObject(sw,entity));
		if (aAF) {
		    cfs_faces.push_back(aAF);
		} else {
		    l->clear();
		    delete l;
		    sw->entity_status[id] = STEP_LOAD_ERROR;
		    return false;
		}
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'cfs_faces'." << std::endl;
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
ConnectedFaceSet::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "cfs_faces:" << std::endl;
    LIST_OF_FACES::iterator i;
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    TopologicalRepresentationItem::Print(level + 1);
}

void
ConnectedFaceSet::ReverseFaceSet()
{
    LIST_OF_FACES::iterator i;
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	(*i)->ReverseFace();
    }
}

STEPEntity *
ConnectedFaceSet::GetInstance(STEPWrapper *sw, int id)
{
    return new ConnectedFaceSet(sw, id);
}

STEPEntity *
ConnectedFaceSet::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

#ifdef _DEBUG_TESTING_
  static int _face_cnt_ = 0;
#endif

bool
ConnectedFaceSet::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    LIST_OF_FACES::iterator i;
    int facecnt = 0;
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
#ifdef _DEBUG_TESTING_
	if (facecnt != _face_cnt_) {
	    facecnt++;
	    continue;
	    std::cerr << "We're here." << std::endl;
	}
#endif
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
	facecnt++;
    }
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
