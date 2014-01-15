/*                 IntersectionCurve.cpp
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
/** @file step/IntersectionCurve.cpp
 *
 * Routines to convert STEP "IntersectionCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "IntersectionCurve.h"

#define CLASSNAME "IntersectionCurve"
#define ENTITYNAME "Intersection_Curve"
string IntersectionCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)IntersectionCurve::Create);

IntersectionCurve::IntersectionCurve()
{
    step = NULL;
    id = 0;
}

IntersectionCurve::IntersectionCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

IntersectionCurve::~IntersectionCurve()
{
}

bool
IntersectionCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
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
IntersectionCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    SurfaceCurve::Print(level + 1);
}

STEPEntity *
IntersectionCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new IntersectionCurve(sw, id);
}

STEPEntity *
IntersectionCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
IntersectionCurve::LoadONBrep(ON_Brep *brep)
{
    bool status;

    if (!brep) {
	return false;
    }

    curve_3d->Start(start);
    curve_3d->End(end);

    status = curve_3d->LoadONBrep(brep);
    ON_id = curve_3d->GetONId();

    return status;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
