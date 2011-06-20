/*                 PCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file step/PCurve.cpp
 *
 * Routines to convert STEP "PCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Surface.h"
#include "DefinitionalRepresentation.h"

#include "PCurve.h"

#define CLASSNAME "PCurve"
#define ENTITYNAME "Pcurve"
string PCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)PCurve::Create);

PCurve::PCurve() {
	step = NULL;
	id = 0;
	basis_surface = NULL;
	reference_to_curve = NULL;
}

PCurve::PCurve(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	basis_surface = NULL;
	reference_to_curve = NULL;
}

PCurve::~PCurve() {
	basis_surface = NULL;
	reference_to_curve = NULL;
}

bool
PCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Curve::Load(sw,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (basis_surface == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,
				"basis_surface");
		if (entity) {
			basis_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw, entity)); // CreateSurfaceObject(sw, entity));
		} else {
			std::cerr << CLASSNAME
					<< ": Error loading entity attribute 'basis_curve'" << std::endl;
			return false;
		}
	}

	if (reference_to_curve == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"reference_to_curve");
		if (entity) {
			reference_to_curve = (DefinitionalRepresentation*) Factory::CreateObject(sw, entity);
		} else {
			std::cerr << CLASSNAME << ": Error loading entity attribute 'reference_to_curve'" << std::endl;
			return false;
		}
	}

	return true;
}

const double *
PCurve::PointAtEnd() {
	//TODO: complete pcurve support
	std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
PCurve::PointAtStart() {
	//TODO: complete pcurve support
	std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}

void
PCurve::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "basis_surface:" << std::endl;
	basis_surface->Print(level+1);
	TAB(level+1); std::cout << "reference_to_curve:" << std::endl;
	reference_to_curve->Print(level+1);
}
STEPEntity *
PCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		PCurve *object = new PCurve(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw,sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
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
