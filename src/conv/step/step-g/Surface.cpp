/*                 Surface.cpp
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
/** @file step/Surface.cpp
 *
 * Routines to interface to STEP "Surface".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"

#define CLASSNAME "Surface"
#define ENTITYNAME "Surface"
string Surface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Surface::Create);

Surface::Surface()
    : trim_curve_3d_bbox(NULL)
{
    step = NULL;
    id = 0;
}

Surface::Surface(STEPWrapper *sw, int step_id)
    : trim_curve_3d_bbox(NULL)
{
    step = sw;
    id = step_id;
}

Surface::~Surface()
{
    if (trim_curve_3d_bbox)
	delete trim_curve_3d_bbox;
}

bool
Surface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	return false;
    }
    return true;
}

void
Surface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;
}

STEPEntity *
Surface::GetInstance(STEPWrapper *sw, int id)
{
    return new Surface(sw, id);
}

STEPEntity *
Surface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
