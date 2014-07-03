/*                 CylindricalSurface.cpp
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
/** @file step/CylindricalSurface.cpp
 *
 * Routines to interface to STEP "CylindricalSurface".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Axis2Placement3D.h"

#include "CylindricalSurface.h"

#define CLASSNAME "CylindricalSurface"
#define ENTITYNAME "Cylindrical_Surface"
string CylindricalSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CylindricalSurface::Create);

CylindricalSurface::CylindricalSurface()
{
    step = NULL;
    id = 0;
    radius = 0.0;
}

CylindricalSurface::CylindricalSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    radius = 0.0;
}

CylindricalSurface::~CylindricalSurface()
{
}

const double *
CylindricalSurface::GetOrigin()
{
    return position->GetOrigin();
}

const double *
CylindricalSurface::GetNormal()
{
    return position->GetAxis(2);
}

const double *
CylindricalSurface::GetXAxis()
{
    return position->GetXAxis();
}

const double *
CylindricalSurface::GetYAxis()
{
    return position->GetYAxis();
}

bool
CylindricalSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ElementarySurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    radius = step->getRealAttribute(sse, "radius");

    return true;
}

void
CylindricalSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level + 1);
    std::cout << "radius: " << radius << std::endl;

    ElementarySurface::Print(level + 1);
}

STEPEntity *
CylindricalSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new CylindricalSurface(sw, id);
}

STEPEntity *
CylindricalSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
