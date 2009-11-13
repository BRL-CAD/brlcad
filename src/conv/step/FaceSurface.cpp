/*                 FaceSurface.cpp
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
/** @file FaceSurface.cpp
 *
 * Routines to convert STEP "FaceSurface" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "FaceSurface.h"
#include "QuasiUniformSurface.h"
#include "FaceOuterBound.h"

#define CLASSNAME "FaceSurface"
#define ENTITYNAME "Face_Surface"
string FaceSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)FaceSurface::Create);

FaceSurface::FaceSurface() {
	step = NULL;
	id = 0;
	face_geometry = NULL;
}

FaceSurface::FaceSurface(STEPWrapper *sw, int STEPid) {
	step = sw;
	id = STEPid;
	face_geometry = NULL;
}

FaceSurface::~FaceSurface() {
	face_geometry=NULL;
}

bool
FaceSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {

	step=sw;
	id = sse->STEPfile_id;


	if ( !Face::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Face." << endl;
		return false;
	}

	if ( !GeometricRepresentationItem::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (face_geometry == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"face_geometry");
		if (entity) {
			//face_geometry = dynamic_cast<Surface *>(Factory::CreateGeometricRepresentationItemObject(sw,entity));
			face_geometry = dynamic_cast<Surface *>(Factory::CreateObject(sw,entity)); //CreateSurfaceObject(sw,entity));
			//face_geometry->Print(0);
		}
	}

	same_sense = step->getBooleanAttribute(sse,"same_sense");

	return true;
}

void
FaceSurface::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "face_geometry:" << endl;
	face_geometry->Print(level+1);

	TAB(level+1); cout << "same_sense:" << step->getBooleanString((SCLBOOL_H(Bool))same_sense) << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	Face::Print(level+1);
	GeometricRepresentationItem::Print(level+1);
}

STEPEntity *
FaceSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		FaceSurface *object = new FaceSurface(sw,sse->STEPfile_id);

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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
