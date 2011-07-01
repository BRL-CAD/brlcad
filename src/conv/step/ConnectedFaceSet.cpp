/*                 ConnectedFaceSet.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
string ConnectedFaceSet::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)ConnectedFaceSet::Create);

ConnectedFaceSet::ConnectedFaceSet() {
	step = NULL;
	id = 0;
}

ConnectedFaceSet::ConnectedFaceSet(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

ConnectedFaceSet::~ConnectedFaceSet() {
	/*
	LIST_OF_FACES::iterator i = cfs_faces.begin();

	while(i != cfs_faces.end()) {
		delete (*i);
		i = cfs_faces.erase(i);
	}
	*/
	cfs_faces.clear();
}

bool
ConnectedFaceSet::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !TopologicalRepresentationItem::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (cfs_faces.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"cfs_faces");
		LIST_OF_ENTITIES::iterator i;
		for(i=l->begin();i!=l->end();i++) {
			SCLP23(Application_instance) *entity = (*i);
			if (entity) {
				Face *aAF = dynamic_cast<Face *>(Factory::CreateObject(sw,entity)); //CreateSurfaceObject(sw,entity));

				cfs_faces.push_back(aAF);
			} else {
				std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'cfs_faces'." << std::endl;
				return false;
			}
		}
		l->clear();
		delete l;
	}

	return true;
}

void
ConnectedFaceSet::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "cfs_faces:" << std::endl;
	LIST_OF_FACES::iterator i;
	for(i=cfs_faces.begin(); i != cfs_faces.end(); ++i) {
		(*i)->Print(level+1);
	}

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	TopologicalRepresentationItem::Print(level+1);
}

STEPEntity *
ConnectedFaceSet::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		ConnectedFaceSet *object = new ConnectedFaceSet(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

bool
ConnectedFaceSet::LoadONBrep(ON_Brep *brep)
{
	LIST_OF_FACES::iterator i;
	for(i=cfs_faces.begin(); i != cfs_faces.end(); ++i) {
		if ( !(*i)->LoadONBrep(brep) ) {
			std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
			return false;
		}
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
