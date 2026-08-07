/*                       C s g S o l i d . c p p
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "CsgSolid.h"

#define CLASSNAME "CsgSolid"
#define ENTITYNAME "Csg_Solid"

string CsgSolid::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CsgSolid::Create);

CsgSolid::CsgSolid() { step = NULL; id = 0; }
CsgSolid::CsgSolid(STEPWrapper *sw, int step_id) { step = sw; id = step_id; }
CsgSolid::~CsgSolid() {}

bool
CsgSolid::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;
    if (!SolidModel::Load(sw, sse)) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
CsgSolid::LoadONBrep(ON_Brep *)
{
    return false;
}

STEPEntity *
CsgSolid::GetInstance(STEPWrapper *sw, int id)
{
    return new CsgSolid(sw, id);
}

STEPEntity *
CsgSolid::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
