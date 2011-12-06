/*                 BSplineSurfaceWithKnots.cpp
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
/** @file step/BSplineSurfaceWithKnots.cpp
 *
 * Routines to convert STEP "BSplineSurfaceWithKnots" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BSplineSurfaceWithKnots.h"
#include "CartesianPoint.h"

#define CLASSNAME "BSplineSurfaceWithKnots"
#define ENTITYNAME "B_Spline_Surface_With_Knots"
string BSplineSurfaceWithKnots::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BSplineSurfaceWithKnots::Create);

static const char *Knot_type_string[] = {
    "uniform_knots",
    "unspecified",
    "quasi_uniform_knots",
    "piecewise_bezier_knots",
    "unset"
};

BSplineSurfaceWithKnots::BSplineSurfaceWithKnots() {
    step = NULL;
    id = 0;
}

BSplineSurfaceWithKnots::BSplineSurfaceWithKnots(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

BSplineSurfaceWithKnots::~BSplineSurfaceWithKnots() {
}

bool
BSplineSurfaceWithKnots::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if ( !BSplineSurface::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BSplineCurve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (u_multiplicities.empty()) {
	STEPattribute *attr = step->getAttribute(sse,"u_multiplicities");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	    IntNode *in = (IntNode *)sa->GetHead();

	    while ( in != NULL) {
		u_multiplicities.push_back(in->value);
		in = (IntNode *)in->NextNode();
	    }
	} else {
	    std::cout << CLASSNAME << ": Error loading BSplineSurfaceWithKnots(u_multiplicities)." << std::endl;
	    return false;
	}
    }
    if (v_multiplicities.empty()) {
	STEPattribute *attr = step->getAttribute(sse,"v_multiplicities");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	    IntNode *in = (IntNode *)sa->GetHead();

	    while ( in != NULL) {
		v_multiplicities.push_back(in->value);
		in = (IntNode *)in->NextNode();
	    }
	} else {
	    std::cout << CLASSNAME << ": Error loading BSplineSurfaceWithKnots(v_multiplicities)." << std::endl;
	    return false;
	}
    }
    if (u_knots.empty()) {
	STEPattribute *attr = step->getAttribute(sse,"u_knots");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	    RealNode *rn = (RealNode *)sa->GetHead();

	    while ( rn != NULL) {
		u_knots.push_back(rn->value);
		rn = (RealNode *)rn->NextNode();
	    }
	} else {
	    std::cout << CLASSNAME << ": Error loading BSplineSurfaceWithKnots(knots)." << std::endl;
	    return false;
	}
    }
    if (v_knots.empty()) {
	STEPattribute *attr = step->getAttribute(sse,"v_knots");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	    RealNode *rn = (RealNode *)sa->GetHead();

	    while ( rn != NULL) {
		v_knots.push_back(rn->value);
		rn = (RealNode *)rn->NextNode();
	    }
	} else {
	    std::cout << CLASSNAME << ": Error loading BSplineSurfaceWithKnots(knots)." << std::endl;
	    return false;
	}
    }

    knot_spec = (Knot_type)step->getEnumAttribute(sse,"knot_spec");
    if (knot_spec > Knot_type_unset)
	knot_spec = Knot_type_unset;

    return true;
}

void
BSplineSurfaceWithKnots::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "u_multiplicities:";
    LIST_OF_INTEGERS::iterator ii;
    for(ii=u_multiplicities.begin();ii!=u_multiplicities.end();ii++) {
	std::cout << " " << (*ii);
    }
    std::cout << std::endl;
    TAB(level+1); std::cout << "v_multiplicities:";
    for(ii=v_multiplicities.begin();ii!=v_multiplicities.end();ii++) {
	std::cout << " " << (*ii);
    }
    std::cout << std::endl;

    TAB(level+1); std::cout << "u_knots:";
    LIST_OF_REALS::iterator ir;
    for(ir=u_knots.begin();ir!=u_knots.end();ir++) {
	std::cout << " " << (*ir);
    }
    std::cout << std::endl;
    TAB(level+1); std::cout << "v_knots:";
    for(ir=v_knots.begin();ir!=v_knots.end();ir++) {
	std::cout << " " << (*ir);
    }
    std::cout << std::endl;

    TAB(level+1); std::cout << "knot_spec:" << Knot_type_string[knot_spec] << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    BSplineSurface::Print(level+1);
}
STEPEntity *
BSplineSurfaceWithKnots::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	BSplineSurfaceWithKnots *object = new BSplineSurfaceWithKnots(sw,sse->STEPfile_id);

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
