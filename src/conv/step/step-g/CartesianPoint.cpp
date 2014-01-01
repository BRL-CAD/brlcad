/*                 CartesianPoint.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/CartesianPoint.cpp
 *
 * Routines to interface to STEP "CartesianPoint".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "vmath.h"
#include "CartesianPoint.h"

#define CLASSNAME "CartesianPoint"
#define ENTITYNAME "Cartesian_Point"
string CartesianPoint::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CartesianPoint::Create);

CartesianPoint::CartesianPoint()
{
    step = NULL;
    id = 0;
    vertex_index = -1;
    coordinates[0] = coordinates[1] = coordinates[2] = 0.0;
}

CartesianPoint::CartesianPoint(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    vertex_index = -1;
    coordinates[0] = coordinates[1] = coordinates[2] = 0.0;
}

CartesianPoint::~CartesianPoint()
{
}

bool
CartesianPoint::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;


    // load base class attributes
    if (!Point::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Point." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    STEPattribute *attr = step->getAttribute(sse, "coordinates");
    if (attr != NULL) {
	STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	RealNode *rn = (RealNode *)sa->GetHead();
	int index = 0;
	while ((rn != NULL) && (index < 3)) {
	    coordinates[index++] = rn->value;
	    rn = (RealNode *)rn->NextNode();
	}
    } else {
	std::cout << CLASSNAME << ": error loading 'coordinate' attribute." << std::endl;
    }
    return true;
}

void
CartesianPoint::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "coordinate:";
    std::cout << "(" << coordinates[0] << ",";
    std::cout << coordinates[1] << ",";
    std::cout << coordinates[2] << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Point::Print(level + 1);
}

STEPEntity *
CartesianPoint::GetInstance(STEPWrapper *sw, int id)
{
    return new CartesianPoint(sw, id);
}

STEPEntity *
CartesianPoint::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
CartesianPoint::LoadONBrep(ON_Brep *brep)
{
    AddVertex(brep);
    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
