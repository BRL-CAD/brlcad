/*                 DesignContext.cpp
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
/** @file step/DesignContext.cpp
 *
 * Routines to convert STEP "DesignContext" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "DesignContext.h"

#define CLASSNAME "DesignContext"
#define ENTITYNAME "Design_Context"
string DesignContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) DesignContext::Create);

DesignContext::DesignContext()
{
    step = NULL;
    id = 0;
}

DesignContext::DesignContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

DesignContext::~DesignContext()
{
}

bool DesignContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ProductDefinitionContext::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::ProductDefinitionContext." << std::endl;
	return false;
    }

    return true;
}

void DesignContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    ProductDefinitionContext::Print(level + 1);
}

STEPEntity *
DesignContext::GetInstance(STEPWrapper *sw, int id)
{
    return new DesignContext(sw, id);
}

STEPEntity *
DesignContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool DesignContext::LoadONBrep(ON_Brep *brep)
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
