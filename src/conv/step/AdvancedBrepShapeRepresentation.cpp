/*                 AdvancedBrepShapeRepresentation.cpp
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
/** @file step/AdvancedBrepShapeRepresentation.cpp
 *
 * Routines to convert STEP "AdvancedBrepShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

/* interface header */
#include "./AdvancedBrepShapeRepresentation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"
#include "ManifoldSolidBrep.h"
#include "GeometricRepresentationContext.h"


#define CLASSNAME "AdvancedBrepShapeRepresentation"
#define ENTITYNAME "Advanced_Brep_Shape_Representation"


std::string AdvancedBrepShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)AdvancedBrepShapeRepresentation::Create);


AdvancedBrepShapeRepresentation::AdvancedBrepShapeRepresentation() {
    context_of_items.clear();
    items.clear();
    step = NULL;
    id = 0;
}


AdvancedBrepShapeRepresentation::AdvancedBrepShapeRepresentation(STEPWrapper *sw, int step_id) {
    context_of_items.clear();
    items.clear();
    step = sw;
    id = step_id;
}


AdvancedBrepShapeRepresentation::~AdvancedBrepShapeRepresentation() {
}


STEPEntity *
AdvancedBrepShapeRepresentation::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	AdvancedBrepShapeRepresentation *object = new AdvancedBrepShapeRepresentation(sw, sse->STEPfile_id);

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
AdvancedBrepShapeRepresentation::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    step = sw;
    id = sse->STEPfile_id;

    if (!ShapeRepresentation::Load(step, sse)) {
	std::cerr << CLASSNAME << ":Error loading base class ::ShapeRepresentation." << std::endl;
	return false;
    }
    return true;
}


void
AdvancedBrepShapeRepresentation::Print(int level) {
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ShapeRepresentation::Print(level);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
