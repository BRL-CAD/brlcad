/*                 SurfaceOfLinearExtrusion.cpp
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
/** @file step/SurfaceOfLinearExtrusion.cpp
 *
 * Routines to interface to STEP "SurfaceOfLinearExtrusion".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "SurfaceOfLinearExtrusion.h"

#include "Vector.h"

#define CLASSNAME "SurfaceOfLinearExtrusion"
#define ENTITYNAME "Surface_Of_Linear_Extrusion"
string SurfaceOfLinearExtrusion::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SurfaceOfLinearExtrusion::Create);

SurfaceOfLinearExtrusion::SurfaceOfLinearExtrusion()
{
    step = NULL;
    id = 0;
    extrusion_axis = NULL;
}

SurfaceOfLinearExtrusion::SurfaceOfLinearExtrusion(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    extrusion_axis = NULL;
}

SurfaceOfLinearExtrusion::~SurfaceOfLinearExtrusion()
{
}

bool
SurfaceOfLinearExtrusion::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!SweptSurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (extrusion_axis == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "extrusion_axis");
	if (entity) {
	    extrusion_axis = dynamic_cast<Vector *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'extrusion_axis' attribute." << std::endl;
	    return false;
	}
    }

    return true;
}

void
SurfaceOfLinearExtrusion::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    if (extrusion_axis != NULL) {
	extrusion_axis->Print(level + 1);
    }

    SweptSurface::Print(level + 1);
}

STEPEntity *
SurfaceOfLinearExtrusion::GetInstance(STEPWrapper *sw, int id)
{
    return new SurfaceOfLinearExtrusion(sw, id);
}

STEPEntity *
SurfaceOfLinearExtrusion::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
