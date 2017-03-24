/*                 FoundedItem.cpp
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
/** @file step/FoundedItem.cpp
 *
 * Routines to convert STEP "FoundedItem" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "FoundedItem.h"

#define CLASSNAME "FoundedItem"
#define ENTITYNAME "Founded_Item"
string FoundedItem::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FoundedItem::Create);

FoundedItem::FoundedItem()
{
    step = NULL;
    id = 0;
}

FoundedItem::FoundedItem(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

FoundedItem::~FoundedItem()
{
}

bool
FoundedItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
FoundedItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;
}

STEPEntity *
FoundedItem::GetInstance(STEPWrapper *sw, int id)
{
    return new FoundedItem(sw, id);
}

STEPEntity *
FoundedItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
