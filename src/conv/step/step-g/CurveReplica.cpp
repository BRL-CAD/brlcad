/*                 CurveReplica.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
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
/** @file step/CurveReplica.cpp
 *
 * Routines to convert STEP "CurveReplica" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator.h"

#include "CurveReplica.h"

#define CLASSNAME "CurveReplica"
#define ENTITYNAME "Curve_Replica"
string CurveReplica::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CurveReplica::Create);

CurveReplica::CurveReplica()
{
    step = NULL;
    id = 0;
    parent_curve = NULL;
    transformation = NULL;
}

CurveReplica::CurveReplica(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    parent_curve = NULL;
    transformation = NULL;
}

CurveReplica::~CurveReplica()
{
}

bool
CurveReplica::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Curve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (parent_curve == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "parent_curve");
	if (entity != NULL) {
	    parent_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw, entity)); //CreateCurveObject(sw,entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading member field \"parent_curve\"." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    if (transformation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "transformation");
	if (entity != NULL) {
	    transformation = dynamic_cast<CartesianTransformationOperator *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading member field \"transformation\"." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

const double *
CurveReplica::PointAtEnd()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
    return NULL;
}

const double *
CurveReplica::PointAtStart()
{
    std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
    return NULL;
}

void
CurveReplica::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "parent_curve:" << std::endl;
    parent_curve->Print(level + 1);

    TAB(level + 1);
    std::cout << "transformation:" << std::endl;
    transformation->Print(level + 1);

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Curve::Print(level + 1);
}

STEPEntity *
CurveReplica::GetInstance(STEPWrapper *sw, int id)
{
    return new CurveReplica(sw, id);
}

STEPEntity *
CurveReplica::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
CurveReplica::LoadONBrep(ON_Brep *brep)
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
