/*                 TrimmingSelect.cpp
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
/** @file step/TrimmingSelect.cpp
 *
 * Routines to convert STEP "TrimmingSelect" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"
#include "TrimmingSelect.h"

#define CLASSNAME "TrimmingSelect"
#define ENTITYNAME "Trimming_Select"
string TrimmingSelect::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)TrimmingSelect::Create);

const char *trimming_select_type_strings[] = {
    "CARTESIAN_POINT",
    "PARAMETER_VALUE",
    "UNKNOWN",
    NULL
};

TrimmingSelect::TrimmingSelect()
{
    step = NULL;
    id = 0;
    cartesian_point = NULL;
    parameter_value = 0.0;
    type = TrimmingSelect::UNKNOWN;
}

TrimmingSelect::TrimmingSelect(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    cartesian_point = NULL;
    parameter_value = 0.0;
    type = TrimmingSelect::UNKNOWN;
}

TrimmingSelect::~TrimmingSelect()
{
    // created through factory will be deleted there.
    cartesian_point = NULL;
}

double
TrimmingSelect::GetParameterTrim()
{
    return parameter_value;
}

const double *
TrimmingSelect::GetPointTrim()
{
    return cartesian_point->Point3d();
}

bool
TrimmingSelect::IsParameterTrim()
{
    if (type == PARAMETER_VALUE) {
	return true;
    }
    return false;
}

bool
TrimmingSelect::Load(STEPWrapper *sw, SDAI_Select *sse)
{
    step = sw;

    //std::cout << sse->UnderlyingTypeName() << std::endl;
    SdaiTrimming_select *v = (SdaiTrimming_select *)sse;

    if (v->IsCartesian_point()) {
	SdaiCartesian_point *p = *v;
	type = CARTESIAN_POINT;
	cartesian_point = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw, (SDAI_Application_instance *)p));
    } else if (v->IsParameter_value()) {
	type = PARAMETER_VALUE;
	parameter_value = (double)*v;
    }

    return true;
}

void
TrimmingSelect::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << std::endl;
    if (type == CARTESIAN_POINT) {
	TAB(level);
	std::cout << "Type:" << trimming_select_type_strings[type] << " Value:" << std::endl;
	cartesian_point->Print(level + 1);
    } else if (type == PARAMETER_VALUE) {
	TAB(level);
	std::cout << "Type:" << trimming_select_type_strings[type] << " Value:" << parameter_value << std::endl;
    }
}

STEPEntity *
TrimmingSelect::GetInstance(STEPWrapper *sw, int id)
{
    return new TrimmingSelect(sw, id);
}

STEPEntity *
TrimmingSelect::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
