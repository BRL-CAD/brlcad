/*                 CompositeCurveSegment.cpp
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
/** @file step/CompositeCurveSegment.cpp
 *
 * Routines to convert STEP "CompositeCurveSegment" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Curve.h"

#include "CompositeCurveSegment.h"

#define CLASSNAME "CompositeCurveSegment"
#define ENTITYNAME "Composite_Curve_Segment"
string CompositeCurveSegment::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CompositeCurveSegment::Create);

static const char *Transition_code_string[] = {
    "discontinuous",
    "continuous",
    "cont_same_gradient",
    "cont_same_gradient_same_curvature",
    "unset"
};

CompositeCurveSegment::CompositeCurveSegment() {
    step = NULL;
    id = 0;
    parent_curve = NULL;
}

CompositeCurveSegment::CompositeCurveSegment(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    parent_curve = NULL;
}

CompositeCurveSegment::~CompositeCurveSegment() {
}

bool
CompositeCurveSegment::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !FoundedItem::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (parent_curve == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"parent_curve");
	parent_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
    }

    transition = (Transition_code)step->getEnumAttribute(sse,"transition");
    if (transition > Transition_code_unset)
	transition = Transition_code_unset;

    same_sense = step->getBooleanAttribute(sse,"same_sense");

    return true;
}

void
CompositeCurveSegment::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "parent_curve:" << std::endl;
    parent_curve->Print(level+1);
    TAB(level+1); std::cout << "transition:" << Transition_code_string[transition] << std::endl;
    TAB(level+1); std::cout << "same_sense:" << step->getBooleanString(same_sense) << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    FoundedItem::Print(level+1);
}
STEPEntity *
CompositeCurveSegment::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	CompositeCurveSegment *object = new CompositeCurveSegment(sw,sse->STEPfile_id);

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
