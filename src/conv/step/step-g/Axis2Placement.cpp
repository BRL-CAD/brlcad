/*                 Axis2Placement.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/Axis2Placement.cpp
 *
 * Routines to convert STEP "Axis2Placement" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Axis2Placement2D.h"
#include "Axis2Placement3D.h"

#include "Axis2Placement.h"

#define CLASSNAME "Axis2Placement"
#define ENTITYNAME "Axis2_Placement"
string Axis2Placement::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Axis2Placement::Create);

const char *axis2_placement_type_names[] = {
    "AXIS2_PLACEMENT_2D",
    "AXIS2_PLACEMENT_3D",
    "AXIS2_PLACEMENT_UNKNOWN",
    NULL
};

Axis2Placement::Axis2Placement()
{
    step = NULL;
    id = 0;
    value = NULL;
    type = Axis2Placement::UNKNOWN;
}

Axis2Placement::Axis2Placement(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    value = NULL;
    type = Axis2Placement::UNKNOWN;
}

Axis2Placement::~Axis2Placement()
{
}

const double *
Axis2Placement::GetOrigin()
{
    return value->GetOrigin();
}

const double *
Axis2Placement::GetNormal()
{
    return value->GetNormal();
}

const double *
Axis2Placement::GetXAxis()
{
    return value->GetXAxis();
}

const double *
Axis2Placement::GetYAxis()
{
    return value->GetYAxis();
}

bool
Axis2Placement::Load(STEPWrapper *sw, SDAI_Select *sse)
{
    step = sw;

    if (value == NULL) {
	SdaiAxis2_placement *v = (SdaiAxis2_placement *)sse;

	if (v->IsAxis2_placement_2d()) {
	    SdaiAxis2_placement_2d *a2 = *v;
	    type = Axis2Placement::AXIS2_PLACEMENT_2D;
	    value = dynamic_cast<Placement *>(Factory::CreateObject(sw, (SDAI_Application_instance *)a2));
	} else if (v->IsAxis2_placement_3d()) {
	    SdaiAxis2_placement_3d *a3 = *v;
	    type = Axis2Placement::AXIS2_PLACEMENT_3D;
	    value = dynamic_cast<Placement *>(Factory::CreateObject(sw, (SDAI_Application_instance *)a3));
	} else {
	    type = Axis2Placement::UNKNOWN;
	}
    }

    return true;
}

void
Axis2Placement::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    if (type == AXIS2_PLACEMENT_2D) {
	TAB(level + 1);
	std::cout << "Type:" << axis2_placement_type_names[type] << " Value:" << std::endl;
	value->Print(level + 1);
    } else if (type == AXIS2_PLACEMENT_2D) {
	TAB(level + 1);
	std::cout << "Type:" << axis2_placement_type_names[type] << " Value:" << std::endl;
	value->Print(level + 1);
    } else {
	TAB(level + 1);
	std::cout << "Type:" << axis2_placement_type_names[type] << " Value:" << std::endl;
    }
}

STEPEntity *
Axis2Placement::GetInstance(STEPWrapper *sw, int id)
{
    return new Axis2Placement(sw, id);
}

STEPEntity *
Axis2Placement::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
