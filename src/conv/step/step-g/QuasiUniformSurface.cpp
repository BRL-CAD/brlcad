/*                 QuasiUniformSurface.cpp
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
/** @file step/QuasiUniformSurface.cpp
 *
 * Routines to interface to STEP "QuasiUniformSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"
#include "QuasiUniformSurface.h"

#define CLASSNAME "QuasiUniformSurface"
#define ENTITYNAME "Quasi_Uniform_Surface"
string QuasiUniformSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)QuasiUniformSurface::Create);

QuasiUniformSurface::QuasiUniformSurface()
{
    step = NULL;
    id = 0;
}

QuasiUniformSurface::QuasiUniformSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

QuasiUniformSurface::~QuasiUniformSurface()
{
}

bool
QuasiUniformSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!BSplineSurface::Load(sw, sse)) {
	std::cout << "Error loading QuasiUniformSurface." << std::endl;
	return false;
    }
    return true;
}

void
QuasiUniformSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited:" << std::endl;
    BSplineSurface::Print(level + 1);
}

STEPEntity *
QuasiUniformSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new QuasiUniformSurface(sw, id);
}

STEPEntity *
QuasiUniformSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
