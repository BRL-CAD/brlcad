/*                 IntersectionCurve.cpp
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
/** @file IntersectionCurve.cpp
 *
 * Routines to convert STEP "IntersectionCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "IntersectionCurve.h"

#define CLASSNAME "IntersectionCurve"
#define ENTITYNAME "Intersection_Curve"
string IntersectionCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)IntersectionCurve::Create);

IntersectionCurve::IntersectionCurve() {
	step = NULL;
	id = 0;
}

IntersectionCurve::IntersectionCurve(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

IntersectionCurve::~IntersectionCurve() {
}

bool
IntersectionCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !SurfaceCurve::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::SurfaceCurve." << endl;
		return false;
	}

	return true;
}

void
IntersectionCurve::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	SurfaceCurve::Print(level+1);
}

STEPEntity *
IntersectionCurve::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		IntersectionCurve *object = new IntersectionCurve(sw,sse->STEPfile_id);

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
IntersectionCurve::LoadONBrep(ON_Brep *brep)
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
