/*                 Unit.cpp
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
/** @file step/Unit.cpp
 *
 * Routines to convert STEP "Unit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Unit.h"

#define CLASSNAME "Unit"
#define ENTITYNAME "Unit"
string Unit::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Unit::Create);

Unit::Unit()
{
    step = NULL;
    id = 0;
}

Unit::Unit(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

Unit::~Unit()
{
}

bool
Unit::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    return true;
}

void
Unit::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;
}

STEPEntity *
Unit::GetInstance(STEPWrapper *sw, int id)
{
    return new Unit(sw, id);
}

STEPEntity *
Unit::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
