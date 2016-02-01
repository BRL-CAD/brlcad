/*                 RectangularTrimmedSurface.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2016 United States Government as represented by
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
/** @file step/RectangularTrimmedSurface.cpp
 *
 * Routines to interface to STEP "RectangularTrimmedSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "RectangularTrimmedSurface.h"

#define CLASSNAME "RectangularTrimmedSurface"
#define ENTITYNAME "Rectangular_Trimmed_Surface"
string RectangularTrimmedSurface::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RectangularTrimmedSurface::Create);

RectangularTrimmedSurface::RectangularTrimmedSurface()
{
    step = NULL;
    id = 0;
    basis_surface = NULL;
    u1 = 0.0;
    u2 = 0.0;
    v1 = 0.0;
    v2 = 0.0;
    usense = BUnset;
    vsense = BUnset;
}

RectangularTrimmedSurface::RectangularTrimmedSurface(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    basis_surface = NULL;
    u1 = 0.0;
    u2 = 0.0;
    v1 = 0.0;
    v2 = 0.0;
    usense = BUnset;
    vsense = BUnset;
}

RectangularTrimmedSurface::~RectangularTrimmedSurface()
{
}

bool
RectangularTrimmedSurface::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!BoundedSurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (basis_surface == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "basis_surface");
	if (entity) {
	    basis_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw, entity));
	}
	if (!entity || !basis_surface) {
	    std::cerr << CLASSNAME << ": error loading 'basis_surface' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    u1 = step->getRealAttribute(sse, "u1");
    u2 = step->getRealAttribute(sse, "u2");
    v1 = step->getRealAttribute(sse, "v1");
    v2 = step->getRealAttribute(sse, "v2");

    usense = step->getBooleanAttribute(sse, "usense");
    vsense = step->getBooleanAttribute(sse, "vsense");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
RectangularTrimmedSurface::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    basis_surface->Print(level + 1);

    TAB(level + 1);
    std::cout << "u1:" << u1 << std::endl;
    TAB(level + 1);
    std::cout << "u2:" << u2 << std::endl;
    TAB(level + 1);
    std::cout << "v1:" << u1 << std::endl;
    TAB(level + 1);
    std::cout << "v2:" << u2 << std::endl;

    TAB(level + 1);
    std::cout << "usense:" << step->getBooleanString((Boolean)usense) << std::endl;
    TAB(level + 1);
    std::cout << "vsense:" << step->getBooleanString((Boolean)vsense) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    BoundedSurface::Print(level + 1);
}

STEPEntity *
RectangularTrimmedSurface::GetInstance(STEPWrapper *sw, int id)
{
    return new RectangularTrimmedSurface(sw, id);
}

STEPEntity *
RectangularTrimmedSurface::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
RectangularTrimmedSurface::LoadONBrep(ON_Brep *brep)
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
