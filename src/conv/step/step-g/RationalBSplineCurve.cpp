/*                 RationalBSplineCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
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
/** @file step/RationalBSplineCurve.cpp
 *
 * Routines to convert STEP "RationalBSplineCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalBSplineCurve.h"
#include "CartesianPoint.h"

#define CLASSNAME "RationalBSplineCurve"
#define ENTITYNAME "Rational_B_Spline_Curve"
string RationalBSplineCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RationalBSplineCurve::Create);

RationalBSplineCurve::RationalBSplineCurve()
{
    step = NULL;
    id = 0;
}

RationalBSplineCurve::RationalBSplineCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RationalBSplineCurve::~RationalBSplineCurve()
{
    weights_data.clear();
}

bool
RationalBSplineCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!BSplineCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BSplineCurve." << std::endl;
	goto step_error;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (weights_data.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "weights_data");

	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
	    if (!sa) goto step_error;
	    RealNode *rn = (RealNode *)sa->GetHead();

	    while (rn != NULL) {
		weights_data.insert(weights_data.end(), rn->value);
		rn = (RealNode *)rn->NextNode();
	    }
	} else {
	    std::cout << CLASSNAME << ": Error loading RationalBSplineCurve(weights_data)." << std::endl;
	    goto step_error;
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

void
RationalBSplineCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    BSplineCurve::Print(level);

    TAB(level);
    std::cout << "weights_data:";
    LIST_OF_REALS::iterator i;
    for (i = weights_data.begin(); i != weights_data.end(); i++) {
	std::cout << " " << (*i);
    }
    std::cout << std::endl;

}

STEPEntity *
RationalBSplineCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new RationalBSplineCurve(sw, id);
}

STEPEntity *
RationalBSplineCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
