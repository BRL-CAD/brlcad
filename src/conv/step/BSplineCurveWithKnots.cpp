/*                 BSplineCurveWithKnots.cpp
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
/** @file step/BSplineCurveWithKnots.cpp
 *
 * Routines to convert STEP "BSplineCurveWithKnots" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BSplineCurveWithKnots.h"
#include "CartesianPoint.h"

#define CLASSNAME "BSplineCurveWithKnots"
#define ENTITYNAME "B_Spline_Curve_With_Knots"
string BSplineCurveWithKnots::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BSplineCurveWithKnots::Create);

static const char *Knot_type_string[] = {
	"uniform_knots",
	"unspecified",
	"quasi_uniform_knots",
	"piecewise_bezier_knots",
	"unset"
};

BSplineCurveWithKnots::BSplineCurveWithKnots() {
	step = NULL;
	id = 0;
}

BSplineCurveWithKnots::BSplineCurveWithKnots(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

BSplineCurveWithKnots::~BSplineCurveWithKnots() {
}

bool
BSplineCurveWithKnots::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// load base class attributes
	if ( !BSplineCurve::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::BSplineCurve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (knot_multiplicities.empty()) {
		STEPattribute *attr = step->getAttribute(sse,"knot_multiplicities");

		if (attr) {
			STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
			IntNode *in = (IntNode *)sa->GetHead();

			while ( in != NULL) {
				knot_multiplicities.push_back(in->value);
				in = (IntNode *)in->NextNode();
			}
		} else {
			std::cout << CLASSNAME << ": Error loading BSplineCurveWithKnots(knot_multiplicities)." << std::endl;
			return false;
		}
	}

	if (knots.empty()) {
		STEPattribute *attr = step->getAttribute(sse,"knots");
		if (attr) {
			STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
			RealNode *rn = (RealNode *)sa->GetHead();

			while ( rn != NULL) {
				knots.push_back(rn->value);
				rn = (RealNode *)rn->NextNode();
			}
		} else {
			std::cout << CLASSNAME << ": Error loading BSplineCurveWithKnots(knots)." << std::endl;
			return false;
		}
	}

	knot_spec = (Knot_type)step->getEnumAttribute(sse,"knot_spec");
	if (knot_spec > Knot_type_unset)
		knot_spec = Knot_type_unset;

	return true;
}

void
BSplineCurveWithKnots::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "knot_multiplicities:";
	LIST_OF_INTEGERS::iterator ii;
	for(ii=knot_multiplicities.begin();ii!=knot_multiplicities.end();ii++) {
		std::cout << " " << (*ii);
	}
	std::cout << std::endl;

	TAB(level+1); std::cout << "knots:";
	LIST_OF_REALS::iterator ir;
	for(ir=knots.begin();ir!=knots.end();ir++) {
		std::cout << " " << (*ir);
	}
	std::cout << std::endl;

	TAB(level+1); std::cout << "knot_spec:" << Knot_type_string[knot_spec] << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	BSplineCurve::Print(level+1);
}

STEPEntity *
BSplineCurveWithKnots::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BSplineCurveWithKnots *object = new BSplineCurveWithKnots(sw,sse->STEPfile_id);

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
