/*                 FunctionallyDefinedTransformation.cpp
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
/** @file step/FunctionallyDefinedTransformation.cpp
 *
 * Routines to convert STEP "FunctionallyDefinedTransformation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "FunctionallyDefinedTransformation.h"

#define CLASSNAME "FunctionallyDefinedTransformation"
#define ENTITYNAME "Functionally_Defined_Transformation"
string FunctionallyDefinedTransformation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FunctionallyDefinedTransformation::Create);

FunctionallyDefinedTransformation::FunctionallyDefinedTransformation()
{
    step = NULL;
    id = 0;
}

FunctionallyDefinedTransformation::FunctionallyDefinedTransformation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

FunctionallyDefinedTransformation::~FunctionallyDefinedTransformation()
{
}

bool
FunctionallyDefinedTransformation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    if (!Transformation::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Transformation." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    name = step->getStringAttribute(sse, "name");
    description = step->getStringAttribute(sse, "description");

    return true;
}

void
FunctionallyDefinedTransformation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "name:" << name << std::endl;
    TAB(level + 1);
    std::cout << "description:" << description << std::endl;
}

STEPEntity *
FunctionallyDefinedTransformation::GetInstance(STEPWrapper *sw, int id)
{
    return new FunctionallyDefinedTransformation(sw, id);
}

STEPEntity *
FunctionallyDefinedTransformation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
