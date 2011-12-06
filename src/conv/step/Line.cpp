/*                 Line.cpp
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
/** @file step/Line.cpp
 *
 * Routines to convert STEP "Line" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "vmath.h"
#include "CartesianPoint.h"
#include "Direction.h"
#include "Vector.h"

#include "Line.h"

#define CLASSNAME "Line"
#define ENTITYNAME "Line"
string Line::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Line::Create);

Line::Line() {
    step = NULL;
    id = 0;
    pnt = NULL;
    dir = NULL;
}

Line::Line(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    pnt = NULL;
    dir = NULL;
}

Line::~Line() {
}

void
Line::StartPoint(double *p) {
    VMOVE(p,pnt->Coordinates());
    return;
}

void
Line::EndPoint(double *p) {
    double d[3];
    double mag = dir->Magnitude();

    VMOVE(d,dir->Orientation());
    VSCALE(d,d,mag);
    VADD2(p,pnt->Coordinates(),d);
    return;
}

bool
Line::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !Curve::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (pnt == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"pnt");
	pnt = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw,entity));
    }
    if (dir == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"dir");
	dir = dynamic_cast<Vector *>(Factory::CreateObject(sw,entity));
    }
    return true;
}
/*TODO: remove

const double *
Line::PointAtEnd() {
std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
return NULL;
}

const double *
Line::PointAtStart() {
std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
return NULL;
}
*/

void
Line::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    pnt->Print(level+1);
    dir->Print(level+1);

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    Curve::Print(level+1);
}
STEPEntity *
Line::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	Line *object = new Line(sw,sse->STEPfile_id);

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
