/*                 RationalQuasiUniformCurve.cpp
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
string RationalQuasiUniformCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RationalQuasiUniformCurve::Create);

RationalQuasiUniformCurve::RationalQuasiUniformCurve()
{
    step = NULL;
    id = 0;
}

RationalQuasiUniformCurve::RationalQuasiUniformCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RationalQuasiUniformCurve::~RationalQuasiUniformCurve()
{
}

bool
RationalQuasiUniformCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes (no need to add quasi here has no additional attributes)
    if (!RationalBSplineCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RationalBSplineCurve." << std::endl;
	return false;
    }

    return true;
}

void
RationalQuasiUniformCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    RationalBSplineCurve::Print(level);
}

STEPEntity *
RationalQuasiUniformCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new RationalQuasiUniformCurve(sw, id);
}

STEPEntity *
RationalQuasiUniformCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
