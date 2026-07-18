/*            C s g S h a p e R e p r e s e n t a t i o n . c p p
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "CsgShapeRepresentation.h"

#define CLASSNAME "CsgShapeRepresentation"
#define ENTITYNAME "Csg_Shape_Representation"

string CsgShapeRepresentation::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)CsgShapeRepresentation::Create);

CsgShapeRepresentation::CsgShapeRepresentation() { step = NULL; id = 0; }
CsgShapeRepresentation::CsgShapeRepresentation(STEPWrapper *sw, int step_id) { step = sw; id = step_id; }
CsgShapeRepresentation::~CsgShapeRepresentation() {}

bool
CsgShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;
    if (!ShapeRepresentation::Load(sw, sse)) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

STEPEntity *
CsgShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new CsgShapeRepresentation(sw, id);
}

STEPEntity *
CsgShapeRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
