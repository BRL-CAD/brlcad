/*                 SeamCurve.cpp
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
/** @file step/SeamCurve.cpp
 *
 * Routines to convert STEP "SeamCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "SeamCurve.h"

#define CLASSNAME "SeamCurve"
#define ENTITYNAME "Seam_Curve"
string SeamCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SeamCurve::Create);

SeamCurve::SeamCurve()
{
    step = NULL;
    id = 0;
}

SeamCurve::SeamCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

SeamCurve::~SeamCurve()
{
}

bool
SeamCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!SurfaceCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::SurfaceCurve." << std::endl;
	return false;
    }

    return true;
}

void
SeamCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    SurfaceCurve::Print(level + 1);
}

STEPEntity *
SeamCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new SeamCurve(sw, id);
}

STEPEntity *
SeamCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
SeamCurve::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
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
