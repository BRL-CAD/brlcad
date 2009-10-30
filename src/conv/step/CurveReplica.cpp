/*                 CurveReplica.cpp
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
/** @file CurveReplica.cpp
 *
 * Routines to convert STEP "CurveReplica" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator.h"

#include "CurveReplica.h"

#define CLASSNAME "CurveReplica"
#define ENTITYNAME "Curve_Replica"
string CurveReplica::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CurveReplica::Create);

CurveReplica::CurveReplica() {
	step = NULL;
	id = 0;
	parent_curve = NULL;
	transformation = NULL;
}

CurveReplica::CurveReplica(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	parent_curve = NULL;
	transformation = NULL;
}

CurveReplica::~CurveReplica() {
}

bool
CurveReplica::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Curve::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Curve." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (parent_curve == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"parent_curve");
		parent_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
	}

	if (transformation == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"transformation");
		transformation = dynamic_cast<CartesianTransformationOperator *>(Factory::CreateObject(sw,entity));
	}

	return true;
}

const double *
CurveReplica::PointAtEnd() {
	cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
CurveReplica::PointAtStart() {
	cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}

void
CurveReplica::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "parent_curve:" << endl;
	parent_curve->Print(level+1);

	TAB(level+1); cout << "transformation:" << endl;
	transformation->Print(level+1);

	TAB(level); cout << "Inherited Attributes:" << endl;
	Curve::Print(level+1);
}

STEPEntity *
CurveReplica::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CurveReplica *object = new CurveReplica(sw,sse->STEPfile_id);

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
CurveReplica::LoadONBrep(ON_Brep *brep)
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
