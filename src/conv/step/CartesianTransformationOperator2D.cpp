/*                 CartesianTransformationOperator2D.cpp
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
/** @file step/CartesianTransformationOperator2D.cpp
 *
 * Routines to convert STEP "CartesianTransformationOperator2D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianTransformationOperator2D.h"

#define CLASSNAME "CartesianTransformationOperator2D"
#define ENTITYNAME "Cartesian_Transformation_Operator_2d"
string CartesianTransformationOperator2D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CartesianTransformationOperator2D::Create);

CartesianTransformationOperator2D::CartesianTransformationOperator2D() {
    step = NULL;
    id = 0;
}

CartesianTransformationOperator2D::CartesianTransformationOperator2D(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

CartesianTransformationOperator2D::~CartesianTransformationOperator2D() {
}

bool
CartesianTransformationOperator2D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !CartesianTransformationOperator::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::CartesianTransformationOperator." << std::endl;
	return false;
    }

    return true;
}

void
CartesianTransformationOperator2D::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << GeometricRepresentationItem::name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    CartesianTransformationOperator::Print(level+1);
}

STEPEntity *
CartesianTransformationOperator2D::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	CartesianTransformationOperator2D *object = new CartesianTransformationOperator2D(sw,sse->STEPfile_id);

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
CartesianTransformationOperator2D::LoadONBrep(ON_Brep *brep)
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
