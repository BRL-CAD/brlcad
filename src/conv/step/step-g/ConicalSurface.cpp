/*                 ConicalSurface.cpp
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
/** @file step/ConicalSurface.cpp
 *
 * Routines to interface to STEP "ConicalSurface".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Axis2Placement3D.h"

#include "ConicalSurface.h"

#define CLASSNAME "ConicalSurface"
#define ENTITYNAME "Conical_Surface"
string ConicalSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ConicalSurface::Create);

ConicalSurface::ConicalSurface()
{
    step = NULL;
    id = 0;
    radius = 0.0;
    semi_angle = 0.0;
}

ConicalSurface::ConicalSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    radius = 0.0;
    semi_angle = 0.0;
}

ConicalSurface::~ConicalSurface()
{
}

const double *
ConicalSurface::GetOrigin()
{
    return position->GetOrigin();
}

const double *
ConicalSurface::GetNormal()
{
    return position->GetAxis(2);
}

const double *
ConicalSurface::GetXAxis()
{
    return position->GetXAxis();
}

const double *
ConicalSurface::GetYAxis()
{
    return position->GetYAxis();
}

bool
ConicalSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
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
    semi_angle = step->getRealAttribute(sse, "semi_angle");

    return true;
}

void
ConicalSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level + 1);
    std::cout << "radius: " << radius << std::endl;
    TAB(level + 1);
    std::cout << "semi_angle: " << semi_angle << std::endl;

    ElementarySurface::Print(level + 1);
}

STEPEntity *
ConicalSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new ConicalSurface(sw, id);
}

STEPEntity *
ConicalSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
