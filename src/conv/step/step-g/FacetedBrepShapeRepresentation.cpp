/*    F A C E T E D B R E P S H A P E R E P R E S E N T A T I O N . C P P
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
#include "FacetedBrepShapeRepresentation.h"

#define CLASSNAME "FacetedBrepShapeRepresentation"
#define ENTITYNAME "Faceted_Brep_Shape_Representation"

string FacetedBrepShapeRepresentation::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)FacetedBrepShapeRepresentation::Create);

FacetedBrepShapeRepresentation::FacetedBrepShapeRepresentation()
{
}

FacetedBrepShapeRepresentation::FacetedBrepShapeRepresentation(STEPWrapper *sw, int step_id) :
    ShapeRepresentation(sw, step_id)
{
}

FacetedBrepShapeRepresentation::~FacetedBrepShapeRepresentation()
{
}

bool
FacetedBrepShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!ShapeRepresentation::Load(sw, sse)) {
	sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[sse->STEPfile_id] = STEP_LOADED;
    return true;
}

void
FacetedBrepShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":(" << STEPid() << ")" << std::endl;
    ShapeRepresentation::Print(level + 1);
}

STEPEntity *
FacetedBrepShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new FacetedBrepShapeRepresentation(sw, id);
}

STEPEntity *
FacetedBrepShapeRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
