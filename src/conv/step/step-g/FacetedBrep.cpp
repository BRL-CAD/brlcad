/*               F A C E T E D B R E P . C P P
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
#include "FacetedBrep.h"

#define CLASSNAME "FacetedBrep"
#define ENTITYNAME "Faceted_Brep"

string FacetedBrep::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FacetedBrep::Create);

FacetedBrep::FacetedBrep()
{
}

FacetedBrep::FacetedBrep(STEPWrapper *sw, int step_id) : ManifoldSolidBrep(sw, step_id)
{
}

FacetedBrep::~FacetedBrep()
{
}

bool
FacetedBrep::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!ManifoldSolidBrep::Load(sw, sse)) {
	sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[sse->STEPfile_id] = STEP_LOADED;
    return true;
}

void
FacetedBrep::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":(" << STEPid() << ")" << std::endl;
    ManifoldSolidBrep::Print(level + 1);
}

STEPEntity *
FacetedBrep::GetInstance(STEPWrapper *sw, int id)
{
    return new FacetedBrep(sw, id);
}

STEPEntity *
FacetedBrep::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
