/*                 RationalQuasiUniformCurve.cpp
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
/** @file step/RationalQuasiUniformCurve.cpp
 *
 * Routines to convert STEP "RationalQuasiUniformCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RationalQuasiUniformCurve.h"

#define CLASSNAME "RationalQuasiUniformCurve"
#define ENTITYNAME "Rational_Quasi_Uniform_Curve"
string RationalQuasiUniformCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)RationalQuasiUniformCurve::Create);

RationalQuasiUniformCurve::RationalQuasiUniformCurve() {
    step = NULL;
    id = 0;
}

RationalQuasiUniformCurve::RationalQuasiUniformCurve(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

RationalQuasiUniformCurve::~RationalQuasiUniformCurve() {
}

bool
RationalQuasiUniformCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    // load base class attributes (no need to add quasi here has no additional attributes)
    if ( !RationalBSplineCurve::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::RationalBSplineCurve." << std::endl;
	return false;
    }

    return true;
}

void
RationalQuasiUniformCurve::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    RationalBSplineCurve::Print(level);
}
STEPEntity *
RationalQuasiUniformCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	RationalQuasiUniformCurve *object = new RationalQuasiUniformCurve(sw,sse->STEPfile_id);

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
