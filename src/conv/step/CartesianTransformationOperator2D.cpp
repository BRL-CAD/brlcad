/*                 CartesianTransformationOperator2D.cpp
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
/** @file CartesianTransformationOperator2D.cpp
 *
 * Routines to convert STEP "CartesianTransformationOperator2D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianTransformationOperator2D.h"

#define CLASSNAME "CartesianTransformationOperator2D"
#define ENTITYNAME "Cartesian_Transformation_Operator_2d"
string CartesianTransformationOperator2D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CartesianTransformationOperator2D::Create);

CartesianTransformationOperator2D::CartesianTransformationOperator2D() {
	step = NULL;
	id = 0;
}

CartesianTransformationOperator2D::CartesianTransformationOperator2D(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

CartesianTransformationOperator2D::~CartesianTransformationOperator2D() {
}

bool
CartesianTransformationOperator2D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !CartesianTransformationOperator::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::CartesianTransformationOperator." << endl;
		return false;
	}

	return true;
}

void
CartesianTransformationOperator2D::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << GeometricRepresentationItem::name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	CartesianTransformationOperator::Print(level+1);
}

STEPEntity *
CartesianTransformationOperator2D::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CartesianTransformationOperator2D *object = new CartesianTransformationOperator2D(sw,sse->STEPfile_id);

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
CartesianTransformationOperator2D::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}
