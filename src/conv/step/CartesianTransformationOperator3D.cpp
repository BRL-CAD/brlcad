/*                 CartesianTransformationOperator3D.cpp
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
/** @file CartesianTransformationOperator3D.cpp
 *
 * Routines to convert STEP "CartesianTransformationOperator3D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Direction.h"

#include "CartesianTransformationOperator3D.h"

#define CLASSNAME "CartesianTransformationOperator3D"
#define ENTITYNAME "Cartesian_Transformation_Operator_3d"
string CartesianTransformationOperator3D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CartesianTransformationOperator3D::Create);

CartesianTransformationOperator3D::CartesianTransformationOperator3D() {
	step = NULL;
	id = 0;
	axis3 = NULL;
}

CartesianTransformationOperator3D::CartesianTransformationOperator3D(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	axis3 = NULL;
}

CartesianTransformationOperator3D::~CartesianTransformationOperator3D() {
}

bool
CartesianTransformationOperator3D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !CartesianTransformationOperator::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::CartesianTransformationOperator." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (axis3 == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis3");
		if (entity) { //this attribute is optional
			axis3 = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		}
	}

	return true;
}

void
CartesianTransformationOperator3D::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << GeometricRepresentationItem::name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	if (axis3) {
		TAB(level+1); cout << "axis3:" << endl;
		axis3->Print(level+1);
	}

	TAB(level); cout << "Inherited Attributes:" << endl;
	CartesianTransformationOperator::Print(level+1);
}

STEPEntity *
CartesianTransformationOperator3D::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CartesianTransformationOperator3D *object = new CartesianTransformationOperator3D(sw,sse->STEPfile_id);

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
CartesianTransformationOperator3D::LoadONBrep(ON_Brep *brep)
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
