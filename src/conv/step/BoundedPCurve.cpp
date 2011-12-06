/*                 BoundedPCurve.cpp
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
/** @file step/BoundedPCurve.cpp
 *
 * Routines to convert STEP "BoundedPCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "DefinitionalRepresentation.h"

#include "BoundedPCurve.h"

#define CLASSNAME "BoundedPCurve"
#define ENTITYNAME "Bounded_Pcurve"
string BoundedPCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BoundedPCurve::Create);

BoundedPCurve::BoundedPCurve() {
    step = NULL;
    id = 0;
}

BoundedPCurve::BoundedPCurve(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

BoundedPCurve::~BoundedPCurve() {
}

bool
BoundedPCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !PCurve::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::PCurve." << std::endl;
	return false;
    }
    if ( !BoundedCurve::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
	return false;
    }

    return true;
}
const double *
BoundedPCurve::PointAtEnd() {
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
BoundedPCurve::PointAtStart() {
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
BoundedPCurve::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    PCurve::Print(level+1);
    BoundedCurve::Print(level+1);
}

STEPEntity *
BoundedPCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	BoundedPCurve *object = new BoundedPCurve(sw,sse->STEPfile_id);

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

bool
BoundedPCurve::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
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
