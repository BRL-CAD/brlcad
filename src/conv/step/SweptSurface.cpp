/*                 SweptSurface.cpp
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
/** @file step/SweptSurface.cpp
 *
 * Routines to interface to STEP "SweptSurface".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "SweptSurface.h"

#include "Curve.h"

#define CLASSNAME "SweptSurface"
#define ENTITYNAME "Swept_Surface"
string SweptSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SweptSurface::Create);

SweptSurface::SweptSurface() {
	step = NULL;
	id = 0;
	swept_curve = NULL;
}

SweptSurface::SweptSurface(STEPWrapper *sw,int step_id) {
	step=sw;
	id = step_id;
	swept_curve = NULL;
}

SweptSurface::~SweptSurface() {
}

bool
SweptSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Surface::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (swept_curve == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"swept_curve");
		if (entity) {
			swept_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": error loading 'swept_curve' attribute." << std::endl;
			return false;
		}
	}

	return true;
}

void
SweptSurface::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	if (swept_curve != NULL) {
		swept_curve->Print(level+1);
	}
}

STEPEntity *
SweptSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SweptSurface *object = new SweptSurface(sw,sse->STEPfile_id);

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
SweptSurface::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; // already loaded

	if (!swept_curve->LoadONBrep(brep)) {
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
		return false;
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
