/*                 RationalBSplineSurface.cpp
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
/** @file step/RationalBSplineSurface.cpp
 *
 * Routines to convert STEP "RationalBSplineSurface" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalBSplineSurface.h"
#include "CartesianPoint.h"

#define CLASSNAME "RationalBSplineSurface"
#define ENTITYNAME "Rational_B_Spline_Surface"
string RationalBSplineSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)RationalBSplineSurface::Create);

RationalBSplineSurface::RationalBSplineSurface() {
    step = NULL;
    id = 0;
}

RationalBSplineSurface::RationalBSplineSurface(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

RationalBSplineSurface::~RationalBSplineSurface() {
    LIST_OF_LIST_OF_REALS::iterator i = weights_data.begin();
    while (i != weights_data.end()) {
	delete (*i);
	i = weights_data.erase(i);
    }
}

bool
RationalBSplineSurface::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if ( !BSplineSurface::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BSplineSurface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (weights_data.empty()) {
	std::string attrval;
	STEPattribute *attr = step->getAttribute(sse,"weights_data");

	if (attr) {
	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    const char *eaStr;

	    while ( sn != NULL) {
		eaStr = sn->asStr(attrval);
		LIST_OF_REALS *uv = step->parseListOfReals(eaStr);
		weights_data.push_back(uv);
		sn = (STEPnode *)sn->NextNode();
	    }
	    /*
	      RealNode *rn = (RealNode *)sa->GetHead();
	      while ( rn != NULL) {
	      weights_data.insert(weights_data.end(),rn->value);
	      rn = (RealNode *)rn->NextNode();
	      }
	    */
	} else {
	    std::cout << CLASSNAME << ": Error loading RationalBSplineSurface(weights_data)." << std::endl;
	    return false;
	}
    }

    return true;
}

void
RationalBSplineSurface::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    BSplineSurface::Print(level);

    TAB(level); std::cout << "weights_data:" << std::endl;
    LIST_OF_LIST_OF_REALS::iterator i;
    for(i=weights_data.begin();i!=weights_data.end();i++) {
	LIST_OF_REALS::iterator j;
	TAB(level);
	for(j=(*i)->begin();j!=(*i)->end();j++) {
	    std::cout << " " << (*j);
	}
	std::cout << std::endl;
    }
}
STEPEntity *
RationalBSplineSurface::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	RationalBSplineSurface *object = new RationalBSplineSurface(sw,sse->STEPfile_id);

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
