/*                 Axis1Placement.cpp
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
/** @file step/Axis1Placement.cpp
 *
 * Routines to convert STEP "Axis1Placement" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "vmath.h"
#include "CartesianPoint.h"
#include "Direction.h"

#include "Axis1Placement.h"

#define CLASSNAME "Axis1Placement"
#define ENTITYNAME "Axis1_Placement"
string Axis1Placement::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Axis1Placement::Create);

Axis1Placement::Axis1Placement() {
    step = NULL;
    id = 0;
    axis = NULL;
}

Axis1Placement::Axis1Placement(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    axis = NULL;
}

Axis1Placement::~Axis1Placement() {
}

void
Axis1Placement::BuildAxis() {

    if (axis == NULL) {
	VSET(z,1.0,0.0,0.0);
    } else {
	VMOVE(z,axis->DirectionRatios());
	VUNITIZE(z);
    }
    return;
}

const double *
Axis1Placement::GetAxis() {
    return z;
}

const double *
Axis1Placement::GetOrigin() {
    return location->Coordinates();
}

const double *
Axis1Placement::GetNormal() {
    return z;
}

const double *
Axis1Placement::GetXAxis() {
    return z;
}

const double *
Axis1Placement::GetYAxis() {
    return z;
}

bool
Axis1Placement::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !Placement::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Placement." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (axis == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis");
	if (entity) {
	    axis = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
	} else { // optional so no problem if not here
	    axis = NULL;
	}
    }

    BuildAxis();

    return true;
}

void
Axis1Placement::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "ref_direction:" << std::endl;
    if (axis)
	axis->Print(level+1);

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    Placement::Print(level+1);

}

STEPEntity *
Axis1Placement::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	Axis1Placement *object = new Axis1Placement(sw,sse->STEPfile_id);

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

bool
Axis1Placement::LoadONBrep(ON_Brep *brep)
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
