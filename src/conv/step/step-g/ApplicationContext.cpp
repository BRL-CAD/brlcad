/*                 ApplicationContext.cpp
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
/** @file step/ApplicationContext.cpp
 *
 * Routines to convert STEP "ApplicationContext" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ApplicationContext.h"

#define CLASSNAME "ApplicationContext"
#define ENTITYNAME "Application_Context"
string ApplicationContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ApplicationContext::Create);

ApplicationContext::ApplicationContext()
{
    step = NULL;
    id = 0;
    application = "";
}

ApplicationContext::ApplicationContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    application = "";
}

ApplicationContext::~ApplicationContext()
{
}

string ApplicationContext::ClassName()
{
    return entityname;
}

string ApplicationContext::Application()
{
    return application;
}

bool ApplicationContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    application = step->getStringAttribute(sse, "application");

    return true;
}

void ApplicationContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "application:" << application << std::endl;
}

STEPEntity *
ApplicationContext::GetInstance(STEPWrapper *sw, int id)
{
    return new ApplicationContext(sw, id);
}

STEPEntity *
ApplicationContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ApplicationContext::LoadONBrep(ON_Brep *brep)
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
