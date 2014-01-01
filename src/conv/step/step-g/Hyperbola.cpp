/*                 Hyperbola.cpp
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

#include "Vertex.h"
#include "Hyperbola.h"

#define CLASSNAME "Hyperbola"
#define ENTITYNAME "Hyperbola"
string Hyperbola::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Hyperbola::Create);

Hyperbola::Hyperbola()
{
    step = NULL;
    id = 0;
    semi_axis = 0.0;
    semi_imag_axis = 0.0;
}

Hyperbola::Hyperbola(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    semi_axis = 0.0;
    semi_imag_axis = 0.0;
}

Hyperbola::~Hyperbola()
{
}

bool
Hyperbola::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
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

    semi_axis = step->getRealAttribute(sse, "semi_axis");
    semi_imag_axis = step->getRealAttribute(sse, "semi_imag_axis");

    return true;
}

void
Hyperbola::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "semi_axis:" << semi_axis << std::endl;
    TAB(level + 1);
    std::cout << "semi_imag_axis:" << semi_imag_axis << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Conic::Print(level + 1);
}

STEPEntity *
Hyperbola::GetInstance(STEPWrapper *sw, int id)
{
    return new Hyperbola(sw, id);
}

STEPEntity *
Hyperbola::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
