/*                 GeometricRepresentationContext.cpp
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
/** @file step/GeometricRepresentationContext.cpp
 *
 * Routines to convert STEP "GeometricRepresentationContext" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "GeometricRepresentationContext.h"

#define CLASSNAME "GeometricRepresentationContext"
#define ENTITYNAME "Geometric_Representation_Context"
string GeometricRepresentationContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)GeometricRepresentationContext::Create);

GeometricRepresentationContext::GeometricRepresentationContext()
{
    step = NULL;
    id = 0;
    coordinate_space_dimension = 0;
}

GeometricRepresentationContext::GeometricRepresentationContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    coordinate_space_dimension = 0;
}

GeometricRepresentationContext::~GeometricRepresentationContext()
{
}

bool
GeometricRepresentationContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RepresentationContext::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationContext." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    coordinate_space_dimension = step->getIntegerAttribute(sse, "coordinate_space_dimension");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
GeometricRepresentationContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "coordinate_space_dimension:" << coordinate_space_dimension << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RepresentationContext::Print(level + 1);
}

STEPEntity *
GeometricRepresentationContext::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricRepresentationContext(sw, id);
}

STEPEntity *
GeometricRepresentationContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
