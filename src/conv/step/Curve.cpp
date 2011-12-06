/*                 Curve.cpp
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
/** @file step/Curve.cpp
 *
 * Routines to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Vertex.h"
#include "Curve.h"

#define CLASSNAME "Curve"
#define ENTITYNAME "Curve"
string Curve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Curve::Create);

Curve::Curve() {
    step = NULL;
    id = 0;
    trimmed = false;
    parameter_trim = false;
    for(int i =0; i<3; i++) {
	trim_startpoint[i] = 0.0;
	trim_endpoint[i] = 0.0;
    }
}

Curve::Curve(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    trimmed = false;
    parameter_trim = false;
    for(int i =0; i<3; i++) {
	trim_startpoint[i] = 0.0;
	trim_endpoint[i] = 0.0;
    }
}

Curve::~Curve() {
}

bool
Curve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !GeometricRepresentationItem::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	return false;
    }

    return true;
}

const double *
Curve::PointAtEnd() {
    if (trimmed) { //explicitly trimmed
	return trim_endpoint;
    } else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
	return end->Point3d();
    } else {
	//std::cerr << "Error: endpoints not specified for curve " << entityname << std::endl;
	return NULL;
    }
}

const double *
Curve::PointAtStart() {
    if (trimmed) { //explicitly trimmed
	return trim_startpoint;
    } else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
	return start->Point3d();
    } else {
	//std::cerr << "Error: endpoints not specified for curve " << entityname << std::endl;
	return NULL;
    }
}

void
Curve::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    GeometricRepresentationItem::Print(level+1);
}

void
Curve::SetParameterTrim(double start_param, double end_param) {
    trimmed = true;
    parameter_trim = true;
    t = start_param;
    s = end_param;
}

void
Curve::SetPointTrim(const double *start_point, const double *end_point) {
    trimmed = true;
    parameter_trim = false;
    for (int i=0;i<3;i++) {
	trim_startpoint[i] = start_point[i];
	trim_endpoint[i] = end_point[i];
    }
}

void
Curve::Start(Vertex *v) {
    trimmed=false;
    start = v;
}

void
Curve::End(Vertex *v) {
    trimmed=false;
    end = v;
}

STEPEntity *
Curve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	Curve *object = new Curve(sw,sse->STEPfile_id);

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
Curve::LoadONBrep(ON_Brep *brep)
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
