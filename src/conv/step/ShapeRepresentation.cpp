/*                 ShapeRepresentation.cpp
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
/** @file step/ShapeRepresentation.cpp
 *
 * Routines to convert STEP "ShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

/* inteface header */
#include "ShapeRepresentation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"
#include "ManifoldSolidBrep.h"
#include "GeometricRepresentationContext.h"


#define BUFSIZE 255 // used for buffer size that stringifies step ID
#define CLASSNAME "ShapeRepresentation"
#define ENTITYNAME "Shape_Representation"

std::string ShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ShapeRepresentation::Create);


ShapeRepresentation::ShapeRepresentation() {
    step = NULL;
    id = 0;
}


ShapeRepresentation::ShapeRepresentation(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}


ShapeRepresentation::~ShapeRepresentation() {
}


bool
ShapeRepresentation::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !Representation::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading baseclass Representation." << std::endl;
	return false;
    }

    return true;
}


void
ShapeRepresentation::Print(int level) {
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    Representation::Print(level);
}


STEPEntity *
ShapeRepresentation::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	ShapeRepresentation *object = new ShapeRepresentation(sw,sse->STEPfile_id);

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
