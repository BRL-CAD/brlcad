/*        C O M P O U N D R E P R E S E N T A T I O N I T E M . C P P
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
#include "STEPGeneratedAPI.h"
#include "Factory.h"
#include "CompoundRepresentationItem.h"

#include <set>

#define CLASSNAME "CompoundRepresentationItem"
#define ENTITYNAME "Compound_Representation_Item"

string CompoundRepresentationItem::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)CompoundRepresentationItem::Create);

CompoundRepresentationItem::CompoundRepresentationItem()
{
    step = NULL;
    id = 0;
}

CompoundRepresentationItem::CompoundRepresentationItem(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

CompoundRepresentationItem::~CompoundRepresentationItem()
{
    /* Members are owned by STEPWrapper's entity cache. */
    elements.clear();
}

bool
CompoundRepresentationItem::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse ? sse->STEPfile_id : 0;
    if (!sse || !RepresentationItem::Load(sw, sse)) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sse = step->getEntity(sse, ENTITYNAME);
    SDAI_Select *selected = step->getSelectAttribute(sse, "item_element");
    if (!selected || !selected->exists()) {
	sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "missing LIST/SET representation item selection");
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    STEPaggregate *aggregate = brlcad::step::SelectedAggregate(selected);
    if (!aggregate) {
	sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "selected LIST/SET representation item has no aggregate value");
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    std::set<int> seen;
    const std::vector<SDAI_Application_instance *> members =
	brlcad::step::Entities(aggregate);
    for (std::vector<SDAI_Application_instance *>::const_iterator member =
	    members.begin(); member != members.end(); ++member) {
	SDAI_Application_instance *entity = *member;
	if (!entity || entity->STEPfile_id <= 0 ||
		!seen.insert(entity->STEPfile_id).second)
	    continue;
	RepresentationItem *item = dynamic_cast<RepresentationItem *>(
	    Factory::CreateObject(sw, entity));
	if (!item) {
	    sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"COMPOUND_REPRESENTATION_ITEM", "item_element",
		"referenced representation item could not be materialized");
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
	elements.push_back(item);
    }

    if (elements.empty()) {
	sw->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "empty compound representation item");
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

bool
CompoundRepresentationItem::LoadONBrep(ON_Brep *)
{
    /* A compound is a container; callers must convert each member exactly. */
    return false;
}

void
CompoundRepresentationItem::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << "(" << id << ")" << std::endl;
    for (std::vector<RepresentationItem *>::const_iterator item = elements.begin();
	 item != elements.end(); ++item) {
	if (*item) (*item)->Print(level + 1);
    }
}

STEPEntity *
CompoundRepresentationItem::GetInstance(STEPWrapper *sw, int step_id)
{
    return new CompoundRepresentationItem(sw, step_id);
}

STEPEntity *
CompoundRepresentationItem::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
