/*                 RepresentationContext.cpp
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
/** @file step/RepresentationContext.cpp
 *
 * Routines to convert STEP "RepresentationContext" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RepresentationContext.h"

#define CLASSNAME "RepresentationContext"
#define ENTITYNAME "Representation_Context"
string RepresentationContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)RepresentationContext::Create);

RepresentationContext::RepresentationContext()
{
    step = NULL;
    id = 0;
}

RepresentationContext::RepresentationContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

RepresentationContext::~RepresentationContext()
{
}

string
RepresentationContext::GetContextIdentifier()
{
    return context_identifier;
}

bool
RepresentationContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    context_identifier = step->getStringAttribute(sse, "context_identifier");
    context_type = step->getStringAttribute(sse, "context_type");

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
RepresentationContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Local Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "context_identifier:" << context_identifier << std::endl;
    TAB(level + 1);
    std::cout << "context_type:" << context_type << std::endl;

}

STEPEntity *
RepresentationContext::GetInstance(STEPWrapper *sw, int id)
{
    return new RepresentationContext(sw, id);
}

STEPEntity *
RepresentationContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
