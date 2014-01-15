/*                 VertexLoop.cpp
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
/** @file step/VertexLoop.cpp
 *
 * Routines to convert STEP "VertexLoop" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Vertex.h"
#include "VertexLoop.h"

#define CLASSNAME "VertexLoop"
#define ENTITYNAME "Vertex_Loop"
string VertexLoop::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)VertexLoop::Create);

VertexLoop::VertexLoop()
{
    step = NULL;
    id = 0;
    loop_vertex = NULL;
}

VertexLoop::VertexLoop(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    loop_vertex = NULL;
}

VertexLoop::~VertexLoop()
{
}

bool
VertexLoop::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sw || !sse)
	return false;

    step = sw;
    id = sse->STEPfile_id;

    if (!Loop::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Path." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (loop_vertex == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "loop_vertex");
	if (entity) {
	    loop_vertex = dynamic_cast<Vertex *>(Factory::CreateObject(sw, entity)); //CreateCurveObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'loop_vertex'." << std::endl;
	    return false;
	}
    }

    return true;
}

void
VertexLoop::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Local Attributes:" << std::endl;
    loop_vertex->Print(level + 1);

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Loop::Print(level + 1);
}

STEPEntity *
VertexLoop::GetInstance(STEPWrapper *sw, int id)
{
    return new VertexLoop(sw, id);
}

STEPEntity *
VertexLoop::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
