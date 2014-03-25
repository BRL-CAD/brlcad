/*                 CompositeCurve.cpp
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
/** @file step/CompositeCurve.cpp
 *
 * Routines to convert STEP "CompositeCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Curve.h"
#include "CompositeCurveSegment.h"

#include "CompositeCurve.h"

#define CLASSNAME "CompositeCurve"
#define ENTITYNAME "Composite_Curve"
string CompositeCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CompositeCurve::Create);

CompositeCurve::CompositeCurve()
{
    step = NULL;
    id = 0;
    self_intersect = LUnset;
}

CompositeCurve::CompositeCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    self_intersect = LUnset;
}

CompositeCurve::~CompositeCurve()
{
    // elements created through factory will be deleted there.
    segments.clear();
}

bool
CompositeCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!BoundedCurve::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (segments.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "segments");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		CompositeCurveSegment *aCCS = dynamic_cast<CompositeCurveSegment *>(Factory::CreateObject(sw, entity));

		segments.push_back(aCCS);
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'segments'." << std::endl;
		l->clear();
		delete l;
		return false;
	    }
	}
	l->clear();
	delete l;
    }
    self_intersect = step->getLogicalAttribute(sse, "self_intersect");

    return true;
}

const double *
CompositeCurve::PointAtEnd()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
CompositeCurve::PointAtStart()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
CompositeCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level);
    std::cout << "segments:" << std::endl;
    LIST_OF_SEGMENTS::iterator i;
    for (i = segments.begin(); i != segments.end(); i++) {
	(*i)->Print(level + 1);
    }
    TAB(level);
    std::cout << "self_intersect:" << step->getLogicalString(self_intersect) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    BoundedCurve::Print(level + 1);
}

STEPEntity *
CompositeCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new CompositeCurve(sw, id);
}

STEPEntity *
CompositeCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
