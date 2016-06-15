/*                 Parabola.cpp
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
/** @file step/Curve.cpp
 *
 * Routines to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Parabola.h"

#define CLASSNAME "Parabola"
#define ENTITYNAME "Parabola"
string Parabola::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Parabola::Create);

Parabola::Parabola()
{
    step = NULL;
    id = 0;
    focal_dist = 0.0;
}

Parabola::Parabola(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    focal_dist = 0.0;
}

Parabola::~Parabola()
{
}

bool
Parabola::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Conic::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Conic." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    focal_dist = step->getRealAttribute(sse, "focal_dist");

    return true;
}

void
Parabola::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "focal_dist:" << focal_dist << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Conic::Print(level + 1);
}

STEPEntity *
Parabola::GetInstance(STEPWrapper *sw, int id)
{
    return new Parabola(sw, id);
}

STEPEntity *
Parabola::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
