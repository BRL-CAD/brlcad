/*                  M A P P E D I T E M . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "MappedItem.h"
#include "RepresentationMap.h"

#define CLASSNAME "MappedItem"
#define ENTITYNAME "Mapped_Item"

string MappedItem::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)MappedItem::Create);

MappedItem::MappedItem() : mapping_source(NULL), mapping_target(NULL)
{
    step = NULL;
    id = 0;
}

MappedItem::MappedItem(STEPWrapper *sw, int step_id) : mapping_source(NULL), mapping_target(NULL)
{
    step = sw;
    id = step_id;
}

MappedItem::~MappedItem()
{
    mapping_source = NULL;
    mapping_target = NULL;
}

bool
MappedItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;
    if (!RepresentationItem::Load(sw, sse)) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sse = step->getEntity(sse, ENTITYNAME);

    SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapping_source");
    mapping_source = entity ? dynamic_cast<RepresentationMap *>(Factory::CreateObject(sw, entity)) : NULL;
    entity = step->getEntityAttribute(sse, "mapping_target");
    mapping_target = entity ? dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw, entity)) : NULL;
    if (!mapping_source || !mapping_target) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
MappedItem::LoadONBrep(ON_Brep *UNUSED(brep))
{
    return false;
}

void
MappedItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(" << STEPid() << ")" << std::endl;
}

STEPEntity *
MappedItem::GetInstance(STEPWrapper *sw, int id)
{
    return new MappedItem(sw, id);
}

STEPEntity *
MappedItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
