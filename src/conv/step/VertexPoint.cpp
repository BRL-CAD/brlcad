/*                 VertexPoint.cpp
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
/** @file VertexPoint.cpp
 *
 * Routines to convert STEP "VertexPoint" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "VertexPoint.h"
#include "CartesianPoint.h"

#define CLASSNAME "VertexPoint"
#define ENTITYNAME "Vertex_Point"
string VertexPoint::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)VertexPoint::Create);

VertexPoint::VertexPoint() {
	step = NULL;
	id = 0;
	vertex_geometry = NULL;
}

VertexPoint::VertexPoint(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	vertex_geometry = NULL;
}

VertexPoint::~VertexPoint() {
	vertex_geometry = NULL;
}

bool
VertexPoint::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// load base class attributes
	if ( !Vertex::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Vertex." << endl;
		return false;
	}
	if ( !GeometricRepresentationItem::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (vertex_geometry == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"vertex_geometry");
		if (entity) {
			vertex_geometry = dynamic_cast<Point *>(Factory::CreateObject(sw,entity));
		} else {
			cout << CLASSNAME << ":Error loading attribute 'vertex_geometry'." << endl;
			return false;
		}
	}
	return true;
}

void
VertexPoint::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "vertex_geometry:" << endl;
	if (vertex_geometry) {
		vertex_geometry->Print(level+1);
	} else {
		TAB(level); cout << "vertex_geometry:NULL" << endl;
	}
}

STEPEntity *
VertexPoint::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {

		VertexPoint *object = new VertexPoint(sw,sse->STEPfile_id);

		Factory::AddVertex(object);

		if (!object->Load(sw, sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method."
					<< endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

bool
VertexPoint::LoadONBrep(ON_Brep *brep)
{
	if ( !vertex_geometry->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	ON_id = vertex_geometry->GetONId();
	return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
