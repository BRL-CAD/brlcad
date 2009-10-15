/*                 Conic.cpp
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
/** @file Conic.cpp
 *
 * Routines to convert STEP "Conic" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Axis2Placement.h"

#include "Conic.h"

#define CLASSNAME "Conic"
#define ENTITYNAME "Conic"
string Conic::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Conic::Create);

Conic::Conic() {
	step = NULL;
	id = 0;
	position = NULL;
}

Conic::Conic(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	position = NULL;
}

Conic::~Conic() {
	delete position;
}

const double *
Conic::GetOrigin() {
	return position->GetOrigin();
}

const double *
Conic::GetNormal() {
	return position->GetNormal();
}

const double *
Conic::GetXAxis() {
	return position->GetXAxis();
}

const double *
Conic::GetYAxis() {
	return position->GetYAxis();
}

bool
Conic::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !GeometricRepresentationItem::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (position == NULL) {
		SCLP23(Select) *select = step->getSelectAttribute(sse,"position");
		if (select) {
			//if (select->CurrentUnderlyingType() == config_control_designt_axis2_placement) {
				Axis2Placement *aA2P = new Axis2Placement();

				position = aA2P;
				if (!aA2P->Load(step,select)) {
					cout << CLASSNAME << ":Error loading select Axis2Placement from Conic." << endl;
					return false;
				}

			//} else {
			//	cout << CLASSNAME << ":Unexpected select type for 'position' : " << select->UnderlyingTypeName() << endl;
			//	return false;
			//}
		}
	}

	return true;
}

void
Conic::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "position:" << endl;
	position->Print(level+1);

	TAB(level); cout << "Inherited Attributes:" << endl;
	GeometricRepresentationItem::Print(level+1);

}

STEPEntity *
Conic::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Conic *object = new Conic(sw,sse->STEPfile_id);

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
Conic::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}
