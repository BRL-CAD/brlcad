/*                 FacetedBrepShapeRepresentation.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/FacetedBrepShapeRepresentation.cpp
 *
 * Routines to convert STEP "FacetedBrepShapeRepresentation" to BRL-CAD
 * structures.  This is a shape_representation whose items are faceted_brep
 * (planar polyhedral / mesh) solids; the geometry itself is imported as a
 * BoT via FacetedBrep.
 *
 */

/* interface header */
#include "FacetedBrepShapeRepresentation.h"

/* implementation headers */
#include "STEPWrapper.h"
#include "Factory.h"

#define CLASSNAME "FacetedBrepShapeRepresentation"
#define ENTITYNAME "Faceted_Brep_Shape_Representation"

std::string FacetedBrepShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FacetedBrepShapeRepresentation::Create);


FacetedBrepShapeRepresentation::FacetedBrepShapeRepresentation()
{
    step = NULL;
    id = 0;
}


FacetedBrepShapeRepresentation::FacetedBrepShapeRepresentation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}


FacetedBrepShapeRepresentation::~FacetedBrepShapeRepresentation()
{
}


bool
FacetedBrepShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ShapeRepresentation::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading baseclass ShapeRepresentation." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}


void
FacetedBrepShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    ShapeRepresentation::Print(level);
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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
