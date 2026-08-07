/* GeometricallyBoundedWireframeShapeRepresentation.cpp
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

#include "GeometricallyBoundedWireframeShapeRepresentation.h"

#define CLASSNAME "GeometricallyBoundedWireframeShapeRepresentation"
#define ENTITYNAME "Geometrically_Bounded_Wireframe_Shape_Representation"

string GeometricallyBoundedWireframeShapeRepresentation::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)GeometricallyBoundedWireframeShapeRepresentation::Create);

GeometricallyBoundedWireframeShapeRepresentation::GeometricallyBoundedWireframeShapeRepresentation()
{
    step = NULL;
    id = 0;
}

GeometricallyBoundedWireframeShapeRepresentation::GeometricallyBoundedWireframeShapeRepresentation(
    STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

GeometricallyBoundedWireframeShapeRepresentation::~GeometricallyBoundedWireframeShapeRepresentation()
{
}

bool
GeometricallyBoundedWireframeShapeRepresentation::Load(STEPWrapper *sw,
    SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;
    if (!ShapeRepresentation::Load(step, sse)) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
GeometricallyBoundedWireframeShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    return brep != NULL;
}

void
GeometricallyBoundedWireframeShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << "(ID:" << STEPid() << ")" << std::endl;
    ShapeRepresentation::Print(level + 1);
}

STEPEntity *
GeometricallyBoundedWireframeShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricallyBoundedWireframeShapeRepresentation(sw, id);
}

STEPEntity *
GeometricallyBoundedWireframeShapeRepresentation::Create(STEPWrapper *sw,
    SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
