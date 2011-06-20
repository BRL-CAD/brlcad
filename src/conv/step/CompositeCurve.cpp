/*                 CompositeCurve.cpp
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
/** @file step/CompositeCurve.cpp
 *
 * Routines to convert STEP "CompositeCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Curve.h"
#include "CompositeCurveSegment.h"

#include "CompositeCurve.h"

#define CLASSNAME "CompositeCurve"
#define ENTITYNAME "Composite_Curve"
string CompositeCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CompositeCurve::Create);

CompositeCurve::CompositeCurve() {
	step = NULL;
	id = 0;
}

CompositeCurve::CompositeCurve(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

CompositeCurve::~CompositeCurve() {
	/*
	LIST_OF_SEGMENTS::iterator i = segments.begin();

	while(i != segments.end()) {
		delete (*i);
		i = segments.erase(i);
	}
	*/
	segments.clear();
}

bool
CompositeCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !BoundedCurve::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (segments.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"segments");
		LIST_OF_ENTITIES::iterator i;
		for(i=l->begin();i!=l->end();i++) {
			SCLP23(Application_instance) *entity = (*i);
			if (entity) {
				CompositeCurveSegment *aCCS = (CompositeCurveSegment *)Factory::CreateObject(sw,entity);

				segments.push_back(aCCS);
			} else {
				std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'segments'." << std::endl;
				return false;
			}
		}
		l->clear();
		delete l;
	}
	self_intersect = step->getLogicalAttribute(sse,"self_intersect");

	return true;
}

const double *
CompositeCurve::PointAtEnd() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
CompositeCurve::PointAtStart() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}

void
CompositeCurve::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level); std::cout << "segments:" << std::endl;
	LIST_OF_SEGMENTS::iterator i;
	for(i=segments.begin();i!=segments.end();i++) {
		(*i)->Print(level+1);
	}
	TAB(level); std::cout << "self_intersect:" << step->getLogicalString(self_intersect) << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	BoundedCurve::Print(level+1);
}
STEPEntity *
CompositeCurve::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CompositeCurve *object = new CompositeCurve(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
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
