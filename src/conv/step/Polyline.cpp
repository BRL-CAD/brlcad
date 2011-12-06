/*                 Polyline.cpp
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
/** @file step/Polyline.cpp
 *
 * Routines to convert STEP "Polyline" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "CartesianPoint.h"

#include "Polyline.h"

#define CLASSNAME "Polyline"
#define ENTITYNAME "Polyline"
string Polyline::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Polyline::Create);

Polyline::Polyline() {
    step = NULL;
    id = 0;
}

Polyline::Polyline(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

Polyline::~Polyline() {
    /* list clear
       LIST_OF_POINTS::iterator i = points.begin();

       while(i != points.end()) {
       delete (*i);
       i = points.erase(i);
       }
    */
    points.clear();
}

bool
Polyline::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !BoundedCurve::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (points.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"points");
	LIST_OF_ENTITIES::iterator i;
	for(i=l->begin();i!=l->end();i++) {
	    SCLP23(Application_instance) *entity = (*i);
	    if (entity) {
		CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw,entity));

		points.push_back(aCP);
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'points'." << std::endl;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    return true;
}

const double *
Polyline::PointAtEnd() {
    LIST_OF_POINTS::reverse_iterator i = points.rbegin();
    if (i != points.rend()) {
	CartesianPoint *p = (*i);
	return p->Point3d();
    }
    return NULL;
}

const double *
Polyline::PointAtStart() {
    LIST_OF_POINTS::iterator i = points.begin();
    if (i != points.end()) {
	CartesianPoint *p = (*i);
	return p->Point3d();
    }
    return NULL;
}

void
Polyline::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level); std::cout << "segments:" << std::endl;
    LIST_OF_POINTS::iterator i;
    for(i=points.begin();i!=points.end();i++) {
	(*i)->Print(level+1);
    }
}
STEPEntity *
Polyline::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	Polyline *object = new Polyline(sw,sse->STEPfile_id);

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
