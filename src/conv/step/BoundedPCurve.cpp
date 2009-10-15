/*                 BoundedPCurve.cpp
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
/** @file BoundedPCurve.cpp
 *
 * Routines to convert STEP "BoundedPCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "DefinitionalRepresentation.h"

#include "BoundedPCurve.h"

#define CLASSNAME "BoundedPCurve"
#define ENTITYNAME "Bounded_Pcurve"
string BoundedPCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BoundedPCurve::Create);

BoundedPCurve::BoundedPCurve() {
	step = NULL;
	id = 0;
}

BoundedPCurve::BoundedPCurve(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

BoundedPCurve::~BoundedPCurve() {
}

bool
BoundedPCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !PCurve::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::PCurve." << endl;
		return false;
	}
	if ( !BoundedCurve::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << endl;
		return false;
	}

	return true;
}
const double *
BoundedPCurve::PointAtEnd() {
	cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
BoundedPCurve::PointAtStart() {
	cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}

void
BoundedPCurve::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	PCurve::Print(level+1);
	BoundedCurve::Print(level+1);
}

STEPEntity *
BoundedPCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BoundedPCurve *object = new BoundedPCurve(sw,sse->STEPfile_id);

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
BoundedPCurve::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}
