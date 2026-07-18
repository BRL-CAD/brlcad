/*                     S W E P T A R E A S O L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "SweptAreaSolid.h"

#define CLASSNAME "SweptAreaSolid"
#define ENTITYNAME "Swept_Area_Solid"

string SweptAreaSolid::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)SweptAreaSolid::Create);

SweptAreaSolid::SweptAreaSolid() { step = NULL; id = 0; }
SweptAreaSolid::SweptAreaSolid(STEPWrapper *sw, int step_id) { step = sw; id = step_id; }
SweptAreaSolid::~SweptAreaSolid() {}

bool
SweptAreaSolid::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse ? sse->STEPfile_id : 0;
    if (!sse || !SolidModel::Load(sw, sse)) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
SweptAreaSolid::LoadONBrep(ON_Brep *)
{
    return false;
}

STEPEntity *
SweptAreaSolid::GetInstance(STEPWrapper *sw, int step_id)
{
    return new SweptAreaSolid(sw, step_id);
}

STEPEntity *
SweptAreaSolid::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
