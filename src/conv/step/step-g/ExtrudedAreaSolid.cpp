/*                 E X T R U D E D A R E A S O L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "ExtrudedAreaSolid.h"

#define CLASSNAME "ExtrudedAreaSolid"
#define ENTITYNAME "Extruded_Area_Solid"

string ExtrudedAreaSolid::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)ExtrudedAreaSolid::Create);

ExtrudedAreaSolid::ExtrudedAreaSolid() { step = NULL; id = 0; }
ExtrudedAreaSolid::ExtrudedAreaSolid(STEPWrapper *sw, int step_id)
    : SweptAreaSolid(sw, step_id) {}
ExtrudedAreaSolid::~ExtrudedAreaSolid() {}

bool
ExtrudedAreaSolid::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!SweptAreaSolid::Load(sw, sse)) {
	if (sse) sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

STEPEntity *
ExtrudedAreaSolid::GetInstance(STEPWrapper *sw, int step_id)
{
    return new ExtrudedAreaSolid(sw, step_id);
}

STEPEntity *
ExtrudedAreaSolid::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
