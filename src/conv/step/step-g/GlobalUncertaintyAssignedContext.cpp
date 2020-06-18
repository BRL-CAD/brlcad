/*                 GlobalUncertaintyAssignedContext.cpp
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
/** @file step/GlobalUncertaintyAssignedContext.cpp
 *
 * Routines to convert STEP "GlobalUncertaintyAssignedContext" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "UncertaintyMeasureWithUnit.h"
#include "GlobalUncertaintyAssignedContext.h"

#define CLASSNAME "GlobalUncertaintyAssignedContext"
#define ENTITYNAME "Global_Uncertainty_Assigned_Context"
string GlobalUncertaintyAssignedContext::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)GlobalUncertaintyAssignedContext::Create);

GlobalUncertaintyAssignedContext::GlobalUncertaintyAssignedContext()
{
    step = NULL;
    id = 0;
}

GlobalUncertaintyAssignedContext::GlobalUncertaintyAssignedContext(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

GlobalUncertaintyAssignedContext::~GlobalUncertaintyAssignedContext()
{
    // elements created through factory will be deleted there.
    uncertainty.clear();
}

bool
GlobalUncertaintyAssignedContext::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!RepresentationContext::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationContext." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (uncertainty.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "uncertainty");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		UncertaintyMeasureWithUnit *aUMWU = dynamic_cast<UncertaintyMeasureWithUnit *>(Factory::CreateObject(sw, entity));

		uncertainty.push_back(aUMWU);
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'uncertainty'." << std::endl;
		l->clear();
		sw->entity_status[id] = STEP_LOAD_ERROR;
		delete l;
		return false;
	    }
	}
	l->clear();
	delete l;
    }
    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
GlobalUncertaintyAssignedContext::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "uncertainty(list):" << std::endl;
    LIST_OF_UNCERTAINTY_MEASURE_WITH_UNIT::iterator i;
    for (i = uncertainty.begin(); i != uncertainty.end(); i++) {
	(*i)->Print(level + 1);
	std::cout << std::endl;
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    RepresentationContext::Print(level + 1);
}

STEPEntity *
GlobalUncertaintyAssignedContext::GetInstance(STEPWrapper *sw, int id)
{
    return new GlobalUncertaintyAssignedContext(sw, id);
}

STEPEntity *
GlobalUncertaintyAssignedContext::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
