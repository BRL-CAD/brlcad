/*                 BoundedPCurve.cpp
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
/** @file step/BoundedPCurve.cpp
 *
 * Routines to convert STEP "BoundedPCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "DefinitionalRepresentation.h"

#include "BoundedPCurve.h"

#define CLASSNAME "BoundedPCurve"
#define ENTITYNAME "Bounded_Pcurve"
string BoundedPCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)BoundedPCurve::Create);

BoundedPCurve::BoundedPCurve()
{
    step = NULL;
    id = 0;
}

BoundedPCurve::BoundedPCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

BoundedPCurve::~BoundedPCurve()
{
}

bool
BoundedPCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!PCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::PCurve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    if (!BoundedCurve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}
const double *
BoundedPCurve::PointAtEnd()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
BoundedPCurve::PointAtStart()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
BoundedPCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    PCurve::Print(level + 1);
    BoundedCurve::Print(level + 1);
}

STEPEntity *
BoundedPCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new BoundedPCurve(sw, id);
}

STEPEntity *
BoundedPCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
BoundedPCurve::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
