/*                 SurfaceReplica.cpp
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
string SurfaceReplica::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SurfaceReplica::Create);

SurfaceReplica::SurfaceReplica() {
	step=NULL;
	id = 0;
	parent_surface = NULL;
	transformation = NULL;
}

SurfaceReplica::SurfaceReplica(STEPWrapper *sw,int step_id) {
	step=sw;
	id = step_id;
	parent_surface = NULL;
	transformation = NULL;
}

SurfaceReplica::~SurfaceReplica() {
}

bool
SurfaceReplica::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {

	step=sw;
	id = sse->STEPfile_id;

	if ( !Surface::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (parent_surface == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"parent_surface");
		if (entity) {
			parent_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": error loading 'parent_surface' attribute." << std::endl;
			return false;
		}
	}
	if (transformation == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"transformation");
		if (entity) {
			transformation = dynamic_cast<CartesianTransformationOperator3D *>(Factory::CreateObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": error loading 'transformation' attribute." << std::endl;
			return false;
		}
	}

	return true;
}

void
SurfaceReplica::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "parent_surface:" << std::endl;
	parent_surface->Print(level+1);
	TAB(level+1); std::cout << "transformation:" << std::endl;
	transformation->Print(level+1);

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Surface::Print(level+1);
}

STEPEntity *
SurfaceReplica::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SurfaceReplica *object = new SurfaceReplica(sw,sse->STEPfile_id);

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
SurfaceReplica::LoadONBrep(ON_Brep *brep)
{
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
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
