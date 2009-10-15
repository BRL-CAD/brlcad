/*                 RationalQuasiUniformSurface.cpp
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
/** @file RationalQuasiUniformSurface.cpp
 *
 * Routines to convert STEP "RationalQuasiUniformSurface" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalQuasiUniformSurface.h"

#define CLASSNAME "RationalQuasiUniformSurface"
#define ENTITYNAME "Rational_Quasi_Uniform_Surface"
string RationalQuasiUniformSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)RationalQuasiUniformSurface::Create);

RationalQuasiUniformSurface::RationalQuasiUniformSurface() {
	step = NULL;
	id = 0;
}

RationalQuasiUniformSurface::RationalQuasiUniformSurface(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

RationalQuasiUniformSurface::~RationalQuasiUniformSurface() {
}

bool
RationalQuasiUniformSurface::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// load base class attributes (no need to add quasi here has no additional attributes)
	if ( !RationalBSplineSurface::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::RationalBSplineSurface." << endl;
		return false;
	}

	return true;
}

void
RationalQuasiUniformSurface::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	RationalBSplineSurface::Print(level);
}
STEPEntity *
RationalQuasiUniformSurface::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		RationalQuasiUniformSurface *object = new RationalQuasiUniformSurface(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw,sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method." << endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}
