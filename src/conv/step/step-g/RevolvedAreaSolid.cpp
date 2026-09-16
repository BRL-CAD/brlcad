/*                   R E V O L V E D A R E A S O L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "RevolvedAreaSolid.h"

#define CLASSNAME "RevolvedAreaSolid"
#define ENTITYNAME "Revolved_Area_Solid"

string RevolvedAreaSolid::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)RevolvedAreaSolid::Create);

RevolvedAreaSolid::RevolvedAreaSolid() { step = NULL; id = 0; }
RevolvedAreaSolid::RevolvedAreaSolid(STEPWrapper *sw, int step_id)
    : SweptAreaSolid(sw, step_id) {}
RevolvedAreaSolid::~RevolvedAreaSolid() {}

bool
RevolvedAreaSolid::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!SweptAreaSolid::Load(sw, sse)) {
	if (sse) sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

STEPEntity *
RevolvedAreaSolid::GetInstance(STEPWrapper *sw, int step_id)
{
    return new RevolvedAreaSolid(sw, step_id);
}

STEPEntity *
RevolvedAreaSolid::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
