/*                 BoundedCurve.cpp
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
/** @file BoundedCurve.cpp
 *
 * Routines to convert STEP "BoundedCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundedCurve.h"

#define CLASSNAME "BoundedCurve"
#define ENTITYNAME "Bounded_Curve"
string BoundedCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BoundedCurve::Create);

BoundedCurve::BoundedCurve() {
	step = NULL;
	id = 0;
}

BoundedCurve::BoundedCurve(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

BoundedCurve::~BoundedCurve() {
}

bool
BoundedCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Curve::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Curve." << endl;
		return false;
	}

	return true;
}
const double *
BoundedCurve::PointAtEnd() {
	cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
BoundedCurve::PointAtStart() {
	cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}


void
BoundedCurve::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	Curve::Print(level+1);

}

STEPEntity *
BoundedCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BoundedCurve *object = new BoundedCurve(sw,sse->STEPfile_id);

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

bool
BoundedCurve::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}
