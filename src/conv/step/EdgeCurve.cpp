/*                 EdgeCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2010 United States Government as represented by
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
/** @file EdgeCurve.cpp
 *
 * Routines to convert STEP "EdgeCurve" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Factory.h"

#include "EdgeCurve.h"
#include "VertexPoint.h"
// possible curve types
#include "QuasiUniformCurve.h"

#define CLASSNAME "EdgeCurve"
#define ENTITYNAME "Edge_Curve"
string EdgeCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)EdgeCurve::Create);

EdgeCurve::EdgeCurve() {
	step = NULL;
	id = 0;
	edge_start = NULL;
	edge_end = NULL;
	edge_geometry = NULL;
}

EdgeCurve::EdgeCurve(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	edge_start = NULL;
	edge_end = NULL;
	edge_geometry = NULL;
}

EdgeCurve::~EdgeCurve() {
	edge_geometry = NULL;
}

bool
EdgeCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Edge::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Edge." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (edge_geometry == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"edge_geometry");
		edge_geometry = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
	}
	edge_geometry->Start(edge_start);
	edge_geometry->End(edge_end);

	same_sense = step->getBooleanAttribute(sse,"same_sense");

	return true;
}

void
EdgeCurve::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "edge_geometry:" << std::endl;
	edge_geometry->Print(level+1);

	TAB(level+1); std::cout << "same_sense:" << step->getBooleanString((SCLBOOL_H(Boolean))same_sense) << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Edge::Print(level+1);
}

STEPEntity *
EdgeCurve::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		EdgeCurve *object = new EdgeCurve(sw,sse->STEPfile_id);

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
