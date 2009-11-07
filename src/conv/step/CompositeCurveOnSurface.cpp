/*                 CompositeCurveOnSurface.cpp
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
/** @file CompositeCurveOnSurface.cpp
 *
 * Routines to convert STEP "CompositeCurveOnSurface" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CompositeCurveOnSurface.h"

#define CLASSNAME "CompositeCurveOnSurface"
#define ENTITYNAME "Composite_Curve_On_Surface"
string CompositeCurveOnSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CompositeCurveOnSurface::Create);

CompositeCurveOnSurface::CompositeCurveOnSurface() {
	step = NULL;
	id = 0;
}

CompositeCurveOnSurface::CompositeCurveOnSurface(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

CompositeCurveOnSurface::~CompositeCurveOnSurface() {
}

bool
CompositeCurveOnSurface::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !CompositeCurve::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::CompositeCurve." << endl;
		return false;
	}

	return true;
}

void

CompositeCurveOnSurface::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	CompositeCurve::Print(level+1);
}
STEPEntity *
CompositeCurveOnSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CompositeCurveOnSurface *object = new CompositeCurveOnSurface(sw,sse->STEPfile_id);

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
CompositeCurveOnSurface::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
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
