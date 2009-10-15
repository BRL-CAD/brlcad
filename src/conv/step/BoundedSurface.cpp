/*                 BoundedSurface.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
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
/** @file BoundedSurface.cpp
 *
 * Routines to interface to STEP "BoundedSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundedSurface.h"

#define CLASSNAME "BoundedSurface"
#define ENTITYNAME "Bounded_Surface"
string BoundedSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BoundedSurface::Create);

BoundedSurface::BoundedSurface() {
	step=NULL;
	id = 0;
}

BoundedSurface::BoundedSurface(STEPWrapper *sw,int STEPid) {
	step=sw;
	id = STEPid;
}

BoundedSurface::~BoundedSurface() {
}

bool
BoundedSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Surface::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Surface." << endl;
		return false;
	}

	return true;
}

void
BoundedSurface::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	Surface::Print(level+1);
}

STEPEntity *
BoundedSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BoundedSurface *object = new BoundedSurface(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method." << endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

bool
BoundedSurface::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}

