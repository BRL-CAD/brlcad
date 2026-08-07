/*            R E P R E S E N T A T I O N M A P . C P P
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
#include "Representation.h"
#include "RepresentationItem.h"
#include "RepresentationMap.h"

#define CLASSNAME "RepresentationMap"
#define ENTITYNAME "Representation_Map"

string RepresentationMap::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)RepresentationMap::Create);

RepresentationMap::RepresentationMap() : mapping_origin(NULL), mapped_representation(NULL)
{
    step = NULL;
    id = 0;
}

RepresentationMap::RepresentationMap(STEPWrapper *sw, int step_id) :
    mapping_origin(NULL), mapped_representation(NULL)
{
    step = sw;
    id = step_id;
}

RepresentationMap::~RepresentationMap()
{
    mapping_origin = NULL;
    mapped_representation = NULL;
}

bool
RepresentationMap::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;
    sse = step->getEntity(sse, ENTITYNAME);

    SDAI_Application_instance *entity = step->getEntityAttribute(sse, "mapping_origin");
    mapping_origin = entity ? dynamic_cast<RepresentationItem *>(Factory::CreateObject(sw, entity)) : NULL;
    entity = step->getEntityAttribute(sse, "mapped_representation");
    mapped_representation = entity ? dynamic_cast<Representation *>(Factory::CreateObject(sw, entity)) : NULL;
    if (!mapping_origin || !mapped_representation) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
RepresentationMap::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":(" << STEPid() << ")" << std::endl;
}

STEPEntity *
RepresentationMap::GetInstance(STEPWrapper *sw, int id)
{
    return new RepresentationMap(sw, id);
}

STEPEntity *
RepresentationMap::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
